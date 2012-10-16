<?php

if (!defined('BASEPATH')) exit('No direct script access allowed');

class Protogeni {
	var $cmuri;
	var $certificate;
	var $parsed_certificate;
	var $passphrase;
	var $xmlrpc_server;
	var $server_path;
	var $db;
	var $DEBUG;
	var $MAX_CMS_AGE;
	var $MAX_RESOURCE_AGE;

	public function init() {
		if (!is_php('5.0.0')) {
			trigger_error('Note, CI docs say NOT to get global instance of CI in __construct if running php4... this should be changed to boolean "loaded" member and load as needed', E_USER_WARNING);
		}
		$CI = & get_instance();
		$this->um = & $CI->usermanager;
		$this->lm = & $CI->localmanager;
		$this->user = & $this->um->current_user;

		if (!$this->user->logged) {
			return false; // Nothing to do if we don't have any credentials
		}

		$parsed_certificate = $this->user->get_parsed_certificate();
		$this->xmlrpc_server = array("ch" => "www.emulab.net");
		if ($parsed_certificate != Null) {
			$this->xmlrpc_server["default"] = $parsed_certificate["issuer"]["CN"];
		}
		$this->server_path = array("default" => ":443/protogeni/xmlrpc/");
		$this->db = Null;
		$this->DEBUG = False;
		$this->MAX_CMS_AGE = 60 * 60; // 1 hour
		$this->MAX_RESOURCE_AGE = 20 * 60; // 20 minutes
	}

	private function get_db() {
		$db = & db_get();

		db_create_table("geni_tables", "name TEXT, version INT");
		db_create_table("geni_cache", "last_updated INT, name TEXT, value TEXT");
		db_create_table("geni_cms", "urn TEXT, hrn TEXT, url TEXT, gid TEXT");
		//db_create_table("geni_resources", "url TEXT, data BLOB");
		db_create_table("geni_users", "uid INT AUTO_INCREMENT, username TEXT, passphrase TEXT, certificate TEXT, session_id TEXT, PRIMARY KEY(uid)", 1);
		db_create_table("geni_experiments", "eid INT AUTO_INCREMENT, pid INT, name TEXT, rspec BLOB, status TEXT, short_status TEXT, PRIMARY KEY(eid)", 1);
		db_create_table("geni_projects", "pid INT AUTO_INCREMENT, name TEXT, PRIMARY KEY(pid)");
		db_create_table("geni_uid2pid", "uid INT, pid INT");
		db_create_table("geni_experiment_meta", "eid INT, urn TEXT, uuid TEXT, xml BLOB, PRIMARY KEY(eid)");

		// Clean-up schema with INSERT
		db_update_cache("db_schema");

		return $db;
	}

	private function do_method($module, $method, $params, $uri=Null, $quiet=False, $version=Null) {
		if (empty($this->user->certificate) || empty($this->user->passphrase)) trigger_error("No certificate was defined!", E_USER_ERROR);

		if (empty($uri) && !empty($this->cmuri) && ($module == "cm" || $module == "cmv2")) {
			// No URI was defined, use default CMURI
			$uri = $this->cmuri;
		}

		if (empty($uri)) {
			if (in_array($module, $this->xmlrpc_server)) {
				$addr = $this->xmlrpc_server[$module];
			} else {
				$addr = $this->xmlrpc_server['default'];
			}

			if (in_array($module, $this->server_path)) {
				$path = $this->server_path[$module];
			} else {
				$path = $this->server_path['default'];
			}
			$uri = "https://" . $addr . $path . $module;
		} elseif (!empty($module)) {
			$uri .= "/" . $module;
		}

		if (!empty($version)) {
			$uri .= "/" . $version;
		}

		$scheme = parse_url($uri, PHP_URL_SCHEME);
		if (empty($scheme)) {
			$uri = "https://" . $uri;
		}

		$parsed_url = parse_url($uri);
		if ($scheme == "https") {
			if (empty($parsed_url['port'])) {
				$parsed_url['port'] = 443;
			}
			$CI = & get_instance();
			$uri = $CI->urlparser->implode($parsed_url);
		}

		$xmlrpc_request = xmlrpc_encode_request($method, $params);

		if ($this->DEBUG) {
			print $uri . " " . $method . "\n";
			print htmlentities($xmlrpc_request);
		}
		$xmlrpc_response = $this->httpPost($uri, $xmlrpc_request);
		$response = xmlrpc_decode($xmlrpc_response);

		if ($response && xmlrpc_is_fault($response)) {
			trigger_error("xmlrpc: $response[faultString] ($response[faultCode])");
			return Null;
		}

		if ($this->DEBUG && isset($response['code']) && $response['code'] != 0) {
			trigger_error("xmlrpc: Method " . $method . " returned with code " . $response['code'] . ": " . $response['output']);
			return Null;
		}

		return $response;
	}

	private function httpPost($uri, $data_to_send, $opts=array('headers' => 0), $auth=array('username' => "", 'password' => "", 'type' => "")) {
		$remote = "ssl://" . preg_replace("#.*://([^/]+).*#", "$1", $uri);
		$host = preg_replace("#.*://([^/]+).*#", "$1", $uri);
		$path = preg_replace("#.*://[^/]+(.*)#", "$1", $uri);

		$context = stream_context_create();
		$result = stream_context_set_option($context, 'ssl', 'verify_host', true);
		$result = stream_context_set_option($context, 'ssl', 'passphrase', $this->user->passphrase);
		$result = stream_context_set_option($context, 'ssl', 'local_cert', $this->user->certificate);
		$result = stream_context_set_option($context, 'ssl', 'cafile', $this->user->certificate);
		$fp = stream_socket_client($remote, $err, $errstr, 60, STREAM_CLIENT_CONNECT, $context);

		if (!$fp) {
			trigger_error('httpPost error: ' . $errstr);
			return Null;
		}

		$req = '';
		$req.="POST $path HTTP/1.1\r\n";
		$req.="Host: $host\r\n";
		if ($auth['type'] == 'basic' && !empty($auth['username'])) {
			$req.="Authorization: Basic ";
			$req.=base64_encode($auth['username'] . ':' . $auth['password']) . "\r\n";
		} elseif ($auth['type'] == 'digest' && !empty($auth['username'])) {
			$req.='Authorization: Digest ';
			foreach ($auth as $k => $v) {
				if (empty($k) || empty($v)) continue;
				if ($k == 'password') continue;
				$req.=$k . '="' . $v . '", ';
			}
			$req.="\r\n";
		}
		$req.="Content-type: text/xml\r\n";
		$req.='Content-length: ' . strlen($data_to_send) . "\r\n";
		$req.="Connection: close\r\n\r\n";

		fputs($fp, $req);
		fputs($fp, $data_to_send);

		$res = "";
		while (!feof($fp)) { $res .= fgets($fp, 128); }
		fclose($fp);

		if ($auth['type'] != 'nodigest'
						&& !empty($auth['username'])
						&& $auth['type'] != 'digest' # prev. digest AUTH failed.
						&& preg_match("/^HTTP\/[0-9\.]* 401 /", $res)) {
			if (1 == preg_match("/WWW-Authenticate: Digest ([^\n\r]*)\r\n/Us", $res, $matches)) {
				foreach (split(",", $matches[1]) as $i) {
					$ii = split("=", trim($i), 2);
					if (!empty($ii[1]) && !empty($ii[0])) {
						$auth[$ii[0]] = preg_replace("/^\"/", '', preg_replace("/\"$/", '', $ii[1]));
					}
				}
				$auth['type'] = 'digest';
				$auth['uri'] = $uri;
				$auth['cnonce'] = randomNonce();
				$auth['nc'] = 1;
				$a1 = md5($auth['username'] . ':' . $auth['realm'] . ':' . $auth['password']);
				$a2 = md5('POST' . ':' . $auth['uri']);
				$auth['response'] = md5($a1 . ':'
												. $auth['nonce'] . ':' . $auth['nc'] . ':'
												. $auth['cnonce'] . ':' . $auth['qop'] . ':' . $a2);
				return httpPost($uri, $data_to_send, $opts, $auth);
			}
		}

		if (1 != preg_match("/^HTTP\/[0-9\.]* ([0-9]{3}) ([^\r\n]*)/", $res, $matches)) {
			trigger_error('httpPost: invalid HTTP reply.');
			return Null;
		}

		if ($matches[1] != '200') {
			trigger_error('httpPost: HTTP error: ' . $matches[1] . ' ' . $matches[2]);
			return Null;
		}

		$chunked_data = (preg_match("/Transfer-Encoding: chunked/i", $res));

		if (!$opts['headers']) {
			$res = preg_replace("/^.*\r\n\r\n/Us", '', $res);
		}

		if ($chunked_data) {
			$res = $this->unchunkHttp11($res);
		}

		return $res;
	}

	private function unchunkHttp11($data) {
		$fp = 0;
		$outData = "";
		while ($fp < strlen($data)) {
			$rawnum = substr($data, $fp, strpos(substr($data, $fp), "\r\n") + 2);
			$num = hexdec(trim($rawnum));
			$fp += strlen($rawnum);
			$chunk = substr($data, $fp, $num);
			$outData .= $chunk;
			$fp += strlen($chunk);
		}
		return $outData;
	}

	public function get_slice_urn($slice_name) {
		$hostname = $this->xmlrpc_server['default'];
		$domain = substr($hostname, strpos($hostname, '.') + 1);
		$slice_urn = "urn:publicid:IDN+" . $domain . "+slice+" . $slice_name;
		return $slice_urn;
	}

	public function get_self_credential() {
		$params = array();
		$response = $this->do_method("sa", "GetCredential", $params);
		if (empty($response['value'])) {
			trigger_error("get_self_credential(): Could not get self credential...", E_USER_ERROR);
			return Null;
		}
		return $response['value'];
	}

	public function get_slice_credential($slice, $self_credential) {
		$params = array(
				'credential' => $self_credential,
				'type' => 'Slice',
		);
		if (in_array('urn', $slice)) $params['urn'] = $slice['urn'];
		else $params['uuid'] = $slice['uuid'];
		$response = $this->do_method("sa", "GetCredential", $params);
		if (empty($response['value'])) {
			trigger_error("get_slice_credential(): Could not get slice credential...", E_USER_ERROR);
			return Null;
		}
		return $response['value'];
	}

	public function list_components() {
		$db = & $this->get_db();
		if (db_check_cache("cms", $this->MAX_CMS_AGE)) {
			$res = $db->query("SELECT urn,hrn,url,gid FROM geni_cms");
			$components = $res->result_array();
			$res->free_result();
		} else {
			$params = array('credential' => $this->get_self_credential());
			$response = $this->do_method("ch", "ListComponents", $params);
			if ($response == Null) return Null;
			$components = $response['value'];
			$db->simple_query("DELETE FROM geni_cms");
			foreach ($components as $component) {
				$sql = "INSERT INTO geni_cms (urn,hrn,url,gid) VALUES (";
				$sql .= "'" . $component['urn'] . "',";
				$sql .= "'" . $component['hrn'] . "',";
				$sql .= "'" . $component['url'] . "',";
				$sql .= "'" . $component['gid'] . "'";
				$sql .= ")";
				$db->simple_query($sql);
			}
			db_update_cache("cms");
		}
		return $components;
	}

	public function discover_resources($url, $opts=array()) {
		if (!isset($opts['available'])) $opts['available'] = True;
		if (!isset($opts['compress'])) $opts['compress'] = True;
		if (!isset($opts['raw_xml'])) $opts['raw_xml'] = False;
		if (!isset($opts['force'])) $opts['force'] = False;
		$db = & $this->get_db();
		if ($opts['force'] == False && db_check_cache("resources", $this->MAX_RESOURCE_AGE, $url)) {
			$fn = $this->get_cm_resource_filename($url);
			if (!file_exists($fn)) {
				$opts['force'] = True;
				return $this->discover_resources($url, $opts);
			}
			$fh = fopen($fn, 'r');
			$resources = "";
			while ($line = fread($fh, 8192)) {
				$resources .= $line;
			}
			fclose($fh);
		} else {
			$params = array(
					'credentials' => array($this->get_self_credential()),
					'available' => $opts['available'],
					'compress' => $opts['compress']
			);
			if (substr($url, -2) == "/cm") $url = substr($url, 0, -3);
			$response = $this->do_method("cm", "DiscoverResources", $params, $url, False, "2.0");
			if ($response == Null) return Null;
			if (is_object($response['value'])) $resources = gzuncompress($response['value']->scalar);
			else $resources = $response['value'];

			$fn = $this->get_cm_resource_filename($url);
			if (file_exists($fn)) unlink($fn);
			$fh = fopen($fn, 'w');
			fwrite($fh, $resources);
			fclose($fh);

			db_update_cache("resources", $url);
		}
		if ($opts['raw_xml'] == False) $resources = parse_rspec($resources);
		return $resources;
	}

	public function get_cm_resource_filename($url) {
		$info = $this->get_cm_info($url);
		$urn = preg_replace('/[\:\.\/\\$]/', '_', $info['urn']);
		$fn = $this->CI->config->item('resource_folder') . $urn . ".rspec";
		if (substr($this->certificate, 1, 1) != "/") { //XXX: This will "break" on windows!
			$fn = BASEPATH . $fn;
		}
		return $fn;
	}

	public function get_cert_resource_filename($username) {
		$fn = $this->CI->config->item('resource_folder') . $username . '.pem';
		if (substr($this->certificate, 1, 1) != "/") { //XXX: This will "break" on windows!
			$fn = BASEPATH . $fn;
		}
		return $fn;
	}

	public function get_cm_info($data, $data_type="url") {
		switch ($data_type) {
			case "url":
			case "urn":
			case "hrn":
				break;
			default:
				trigger_error("Invalid data-type for retrieving CM info. Must be one of: url, urn, hrn", E_USER_ERROR);
		}
		$sql = "SELECT urn, hrn, url, gid FROM geni_cms WHERE " . $data_type . "='" . $data . "'";
		$db = & $this->get_db();
		$res = $db->query($sql);
		if ($res->num_rows() != 1) {
			trigger_error("Unexpected result retrieving CM info.. expect exactly 1 row, got " . $res->num_rows(), E_USER_ERROR);
		}
		return $res->row_array();
	}

	// Component Manager //
	public function cm_create_sliver($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['rspec'])) $params['rspec'] = '';
		if (empty($params['keys'])) $params['keys'] = array();
		if (empty($params['credentials'])) $params['credentials'] = array();
		if (empty($params['impotent'])) $params['impotent'] = 1; //XXX: UNDOCUMENTED ARGUMENT! See createsliver.py:113
 return $this->do_method('cm', 'CreateSliver', $params, False, "2.0");
	}

	public function cm_get_ticket($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['rspec'])) $params['rspec'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'GetTicket', $params);
	}

	public function cm_update_ticket($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['ticket'])) $params['ticket'] = '';
		if (empty($params['rspec'])) $params['rspec'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'UpdateTicket', $params);
	}

	public function cm_redeem_ticket($params=array()) {
		if (empty($params['sliver_urn'])) $params['sliver_urn'] = '';
		if (empty($params['rspec'])) $params['rspec'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'RedeemTicket', $params);
	}

	public function cm_renew_slice($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['expiration'])) $params['expiration'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'RenewSlice', $params);
	}

	public function cm_release_ticket($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['ticket'])) $params['ticket'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'ReleaseTicket', $params);
	}

	public function cm_start_sliver($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = ''; // Either slice_urn *OR* sliver_urns, not both
 if (empty($params['sliver_urns'])) $params['sliver_urns'] = array();
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'StartSliver', $params);
	}

	public function cm_stop_sliver($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = ''; // Either slice_urn *OR* sliver_urns, not both
 if (empty($params['sliver_urns'])) $params['sliver_urns'] = array();
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'StopSliver', $params);
	}

	public function cm_restart_sliver($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = ''; // Either slice_urn *OR* sliver_urns, not both
 if (empty($params['sliver_urns'])) $params['sliver_urns'] = array();
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'RestartSliver', $params);
	}

	public function cm_delete_sliver($params=array()) {
		if (empty($params['sliver_urn'])) $params['sliver_urn'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'DeleteSliver', $params, Null, "2.0");
	}

	public function cm_get_sliver($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'GetSliver', $params, Null, "2.0");
	}

	public function cm_bind_to_slice($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['keys'])) $params['keys'] = array();
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'BindToSlice', $params);
	}

	public function cm_sliver_status($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'SliverStatus', $params);
	}

	public function cm_wait_for_status($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['status'])) $params['status'] = '';
		if (empty($params['timeout'])) $params['timeout'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'wait_for_status', $params);
	}

	public function cm_shutdown_slice($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['clear'])) $params['clear'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('cm', 'Shutdown', $params);
	}

	public function cm_get_version($params=array()) {
		return $this->do_method('cm', 'GetVersion', $params);
	}

	// Slice Authority //
	public function sa_delete_slice($params=array()) {
		if (empty($params['slice_urn'])) $params['slice_urn'] = '';
		if (empty($params['credentials'])) $params['credentials'] = array();
		return $this->do_method('sa', 'DeleteSlice', $params);
	}

	public function sa_get_keys($params=array()) {
		if (empty($params['credential'])) $params['credential'] = '';
		return $this->do_method('sa', 'GetKeys', $params);
	}

	public function sa_resolve($params=array()) {
		if (empty($params['credential'])) $params['credential'] = '';
		if (empty($params['type'])) $params['type'] = '';
		// Needs to have one of:
		//  uuid, hrn, urn
		return $this->do_method('sa', 'Resolve', $params);
	}

	public function cm_resolve($params=array()) {
		if (empty($params['credential'])) $params['credential'] = '';
		if (empty($params['type'])) $params['type'] = '';
		// Needs to have one of:
		//  uuid, hrn, urn
		return $this->do_method('cm', 'Resolve', $params, Null, '2.0');
	}

	public function sa_register($params=array()) {
		if (empty($params['credential'])) $params['credential'] = '';
		if (empty($params['type'])) $params['type'] = '';
		// Needs to have one of:
		//  hrn, urn
		return $this->do_method('sa', 'Register', $params);
	}

	public function resolve_slice($slice_name, $credential) {
		$params = array(
				'credential' => $credential,
				'type' => 'Slice',
				'urn' => $this->get_slice_urn($slice_name)
		);
		$res = $this->sa_resolve($params);
		if ($res['code'] != 0) {
			trigger_error("resolve_slice(): ".$res['output'], E_USER_ERROR);
			return;
		}
		return $res['value'];
	}

	public function create_sliver($eid) {
		$exp = $this->lm->get_experiment($eid);
		if (is_string($exp)) return $exp;


		$db = & $this->get_db();

		$slice_name = $exp['name'];
		$self_credential = $this->get_self_credential();

		$slice_urn = $this->get_slice_urn($slice_name);

		echo "Getting keys...<br>";

		$res = $this->sa_get_keys(array('credential' => $self_credential));
		if ($res['code'] != 0) {
			return "Failed to get SSH keys for user";
		}
		$keys = $res['value'];

		$params = array(
				'credential' => $self_credential,
				'type' => 'Slice',
				'hrn' => $slice_name,
		);

		echo "Resolving slice...<br>";

		$res = $this->sa_resolve($params);
		if ($res['code'] != 0) {
			// Slice doesn't exist, create new slice

			echo "Registering slice...<br>";

			$res = $this->sa_register($params);
			$slice_credential = $res['value'];
			$slice['urn'] = $this->get_slice_urn($slice_name);
		} else {
			// Slice exists, get credential for it

			echo "Getting slice credential...<br>";

			$slice = $res['value'];
			$slice_credential = $this->get_slice_credential($slice, $self_credential);
		}
		echo "URN is: ".$slice_urn."<br>";

		echo "Creating sliver...<br>";

		$params = array(
				'slice_urn' => $slice_urn,
				'rspec' => $exp['rspec'],
				'keys' => $keys,
				'credentials' => array($slice_credential),
				'impotent' => 0,
		);
		echo "<TEXTAREA>";
		var_dump($params);
		echo "</TEXTAREA>";
		die();
		$res = $this->cm_create_sliver($params);
		if ($res['code']) {
			trigger_error("Failed to create sliver: \"" . $res['output'] . "\"", E_USER_ERROR);
			return false;
		}
		var_dump($res['value']);

		echo "Sliver created...<br>";
		return true;
	}

	public function sliver_status($eid) {
		$exp = $this->lm->get_experiment($eid);
		if (is_string($exp)) return $exp;

		$db = & $this->get_db();

		$slice_name = $exp['name'];
		$slice_urn = $this->get_slice_urn($slice_name);

		// Self credential
		$self_credential = $this->get_self_credential();

		// lookup slice
		$slice = $this->resolve_slice($slice_name, $self_credential);

		// Get slice credential
		$slice_credential = $this->get_slice_credential($slice, $self_credential);

		// Get sliver credential
		$params = array(
				'slice_urn' => $slice_urn,
				'credentials' => array($slice_credential)
		);
		$res = $this->cm_get_sliver($params);
		if ($res['code'] != 0) {
			return "Error getting sliver credential: ".$res['output'];
		}
		$sliver_credential = $res['value'];

		// Get the sliver status
		$params = array(
				'slice_urn' => $slice_urn,
				'credentials' => array($sliver_credential)
		);
		$res = $this->cm_sliver_status($params);
		if ($res['code'] != 0) {
			return "Could not get sliver status";
		}
		return $res['value'];
	}

	public function start_sliver($eid) {

	}

	public function stop_sliver($eid) {

	}

	public function delete_slice($eid) {
		$exp = $this->lm->get_experiment($eid);
		if (is_string($exp)) return $exp;

		$db = & $this->get_db();

		$slice_name = $exp['name'];
		$slice_urn = $this->get_slice_urn($slice_name);

		// Self credential
		$self_credential = $this->get_self_credential();

		// lookup slice
		$slice = $this->resolve_slice($slice_name, $self_credential);

		// Get slice credential
		$slice_credential = $this->get_slice_credential($slice, $self_credential);

		// Resolve to get the sliver URN
		$params = array(
				'credentials' => array($slice_credential),
				'urn' => $slice['urn']
		);
		$res = $this->cm_resolve($params);
		if ($res['code'] != 0) {
				trigger_error("Unable to resolve slice ".$slice_name.": ".$res['output'], E_USER_ERROR);
				return;
		}
		$slice = $res['value'];
		if (!in_array('sliver_urn', $slice)) {
			return "No sliver exists for slice";
		}

		// Various parts of their code toggle back and forth between a generated URN and the response's URN... wtf
		assert($slice_urn == $slice['urn']);
		// </rant>

		// Get the sliver credential
		$params = array(
				'credentials' => array($slice_credential),
				'slice_urn' => $slice_urn
		);
		$res = $this->cm_get_sliver($params);
		if ($res['code'] != 0) {
			trigger_error("Could not get Sliver credential: ".$res['output'], E_USER_ERROR);
			return;
		}
		$sliver_credential = $res['value'];

		// Delete the sliver
		$params = array(
				'credentials' => array($sliver_credential),
				'sliver_urn' => $slice
		);
		$res = $this->cm_delete_sliver($params);
		if ($res['code'] != 0) {
			trigger_error("Could not delete sliver: ".$res['output'], E_USER_ERROR);
			return;
		}
		return $res['value'];
	}

}

?>
