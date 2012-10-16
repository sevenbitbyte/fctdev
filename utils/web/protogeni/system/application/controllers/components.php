<?php

class Components extends Controller {

	function Components()
	{
		parent::Controller();	
		$this->load->helper('url');
		$this->output->set_header('Last-Modified: '.gmdate('D, d M Y H:i:s', time()).' GMT');
		$this->output->set_header('Expires: '.gmdate('D, d M Y H:i:s', time()).' GMT');
		$this->output->set_header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0, post-check=0, pre-check=0");
		$this->output->set_header("Pragma: no-cache");
		if (!$this->protogeni->user->logged) {
			redirect('/user/login', 'location', 302);
		}
		$this->load->database();
	}
	
	function index()
	{
		$this->load->view('components');
	}

	function _is_json() {
		if (stristr($_SERVER['HTTP_ACCEPT'], "application/json") != False) {
			return true;
		}
		return false;
	}

	function list_cms() {
		$data['components'] = $this->protogeni->list_components();
		if ($this->_is_json()) {
			echo json_encode($data);
		} else {
			$this->load->view('list_components', $data);
		}
	}

	function discover_resources() {
		$data['cm_url'] = $_REQUEST['url'];
		$data['raw_xml'] = False;
		$data['resources'] = $this->protogeni->discover_resources($_REQUEST['url'], array('available'=>False,'raw_xml'=>$data['raw_xml']));
		if ($this->_is_json()) {
			echo json_encode($data);
		} else {
			$this->load->view('resources', $data);
		}
	}
}

/* End of file welcome.php */
/* Location: ./system/application/controllers/welcome.php */
