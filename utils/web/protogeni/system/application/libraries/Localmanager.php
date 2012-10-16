<?php

class Localmanager {

	var $current_user;

	public function __construct() {
		$this->current_user = Null;
	}

	public function init() {
		$CI = & get_instance();
		$this->current_user = $CI->usermanager->current_user;
	}

	public function create_project($name) {
		$db = & db_get();
		$sql = "SELECT pid FROM geni_projects WHERE name=?";
		$res = $db->query($sql, array($name));
		if ($res->num_rows() > 0) return;
		$sql = "INSERT INTO geni_projects (name) VALUES (?)";
		$db->query($sql, array($name));
	}

	public function assign_user_to_project($username, $pid) {
		$db = & db_get();
		$sql = "INSET INTO geni_uid2pid (uid, pid) VALUES((SELECT uid FROM geni_users WHERE username=?), ?)";
		$db->query($sql, array($username, $pid));
	}

	public function list_projects($username) {
		$db = & db_get();
		$sql = "SELECT p.pid AS pid, p.name AS name FROM geni_projects AS p, geni_users AS u, geni_uid2pid AS up ";
		$sql .= "WHERE u.uid = up.uid AND up.pid = p.pid AND u.username = ?";
		$res = $db->query($sql, array($username));
		return $res->result_array();
	}

	public function create_experiment($name, $pid) {
		$rspec = <<<RSPEC
<?xml version="1.0" encoding="UTF-8"?>
<rspec xmlns="http://www.protogeni.net/resources/rspec/0.1"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="http://www.protogeni.net/resources/rspec/0.1 http://www.protogeni.net/resources/rspec/0.1/request.xsd"
  type="request">
</rspec>
RSPEC;
		return $this->import_experiment($name, $rspec, $pid);
	}

	public function import_experiment($name, $rspec, $pid) {
		if (!$this->check_project_permission($pid)) return "Invalid permissions for project";
		if (empty($name)) return "Invalid name for experiment";
		$db = & db_get();
		$sql = "SELECT eid FROM geni_experiments WHERE name=?";
		$res = $db->query($sql, array($name));
		$params = array($name, $rspec, $pid);
		if ($res->num_rows() > 0) {
			$eid = $res->row()->eid;
			$sql = "UPDATE geni_experiments SET name=?, rspec=?, pid=? WHERE eid=?";
			$params[] = $eid;
		} else {
			$sql = "INSERT INTO geni_experiments (name, rspec, pid) VALUES (?, ?, ?)";
		}
		$db->query($sql, $params);
		return true;
	}

	public function update_experiment($eid, $params=Null) {
		$db = & db_get();
		$sql = "SELECT pid FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		if ($res->num_rows() == 0) return "Invalid EID";

		$pid = $res->row()->pid;
		if (!$this->check_project_permission($pid)) return "Invalid permissions for project";


		if ($params == Null) return "No parameters specified to change...";

		$sql = "UPDATE geni_experiments SET ";
		$first_param = True;
		foreach ($params as $param => $value) {
			if (!$first_param) $sql .= ", ";
			else $first_param = False;
			$sql .= $param . "=?";
			$sql_params[] = $value;
		}
		$sql .= " WHERE eid=?";
		$sql_params[] = $eid;
		$db->query($sql, $sql_params);
		return true;
	}

	public function delete_experiment($eid) {
		$db = & db_get();
		$sql = "SELECT pid FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		if ($res->num_rows() == 0) return "Invalid experiment";

		$pid = $res->row()->pid;
		if (!$this->check_project_permission($pid)) return "Invalid permissions for project";

		$res->free_result();

		$sql = "DELETE FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));

		return true;
	}

	public function get_experiment($eid) {
		$db = & db_get();
		$sql = "SELECT pid FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		if ($res->num_rows() == 0) return "Invalid experiment";

		$pid = $res->row()->pid;
		if (!$this->check_project_permission($pid)) return "Invalid permissions for project";

		$sql = "SELECT eid,name,rspec FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		$res = $res->result_array();
		return $res[0];
	}

	public function experiment_status($eid) {
		$db = & db_get();
		$sql = "SELECT pid FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		if ($res->num_rows() == 0) return "Invalid experiment";

		$pid = $res->row()->pid;
		if (!$this->check_project_permission($pid)) return "Invalid permissions for project";

		$sql = "SELECT eid,name,status,short_status FROM geni_experiments WHERE eid=?";
		$res = $db->query($sql, array($eid));
		$res = $res->result_array();
		return $res[0];
	}

	public function list_experiments($username) {
		$db = & db_get();
		$sql = "SELECT e.eid AS eid, e.name AS name, e.status AS status, e.short_status AS short_status FROM geni_experiments AS e, geni_users AS u, geni_uid2pid AS up ";
		$sql .= "WHERE u.uid = up.uid AND up.pid = e.pid AND u.username = ?";
		$res = $db->query($sql, array($username));
		return $res->result_array();
	}

	public function check_project_permission($pid) {
		$db = & db_get();
		$sql = "SELECT u.uid FROM geni_users AS u, geni_uid2pid AS up WHERE u.uid = up.uid AND up.pid = ? AND u.uid = ?";
		$res = $db->query($sql, array($pid, $this->current_user->uid));
		if ($res->num_rows() > 0) return true;
		return false;
	}

}
?>
