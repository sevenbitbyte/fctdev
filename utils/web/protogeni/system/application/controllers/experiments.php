<?php

class Experiments extends Controller {

	function Experiments() {
		parent::Controller();
		$this->load->helper('url');
		$this->output->set_header('Last-Modified: ' . gmdate('D, d M Y H:i:s', time()) . ' GMT');
		$this->output->set_header('Expires: ' . gmdate('D, d M Y H:i:s', time()) . ' GMT');
		$this->output->set_header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0, post-check=0, pre-check=0");
		$this->output->set_header("Pragma: no-cache");
		$this->usermanager->init();
		if (!$this->usermanager->current_user->logged) {
			redirect('/user/login', 'location', 302);
		}
		$this->protogeni->init();
		$this->localmanager->init();
	}

	function index() {
		$data['experiments'] = $this->localmanager->list_experiments($this->usermanager->current_user->username);
		$this->load->view('experiments', $data);
	}

	function _is_json() {
		if (stristr($_SERVER['HTTP_ACCEPT'], "application/json") != False) {
			return true;
		}
		return false;
	}

	function import() {
		if (!isset($_FILES["rspec"])) redirect('/experiments', 'location', 302);

		if (is_uploaded_file($_FILES["rspec"]["tmp_name"])) {
			$name = $_POST['name'];
			$rspec = file_get_contents($_FILES["rspec"]["tmp_name"]);
			$pid = $_POST['project']; //XXX: view needs list of projects to return PID
			$this->localmanager->import_experiment($name, $rspec, $pid);
			unlink($_FILES["rspec"]["tmp_name"]);
			redirect('/experiments', 'location', 302);
		}
	}

	function create() {
		$name = $_POST['name'];
		$pid = $_POST['project']; //XXX: view needs list of projects to return PID
		$this->localmanager->create($name, $pid);
		unlink($_FILES["rspec"]["tmp_name"]);
		redirect('/experiments', 'location', 302);
	}

	function delete($eid=-1) {
		if ($eid > 0) $this->localmanager->delete_experiment($eid);
		redirect('/experiments', 'location', 302);
	}

	function create_sliver($eid=-1) {
		if ($eid > 0) $this->protogeni->create_sliver($eid);
		redirect($_SERVER['HTTP_REFERER'], 'refresh');
	}

	function start_sliver($eid=-1) {
		if ($eid > 0) $this->protogeni->start_sliver($eid);
		redirect($_SERVER['HTTP_REFERER'], 'refresh');
	}

	function stop_sliver($eid=-1) {
		if ($eid > 0) $this->protogeni->stop_sliver($eid);
		redirect($_SERVER['HTTP_REFERER'], 'refresh');
	}

	function sliver_status($eid=-1) {
		if ($eid < 0) {
			redirect($_SERVER['HTTP_REFERER'], 'refresh');
		}
		$data['exp'] = $this->localmanager->experiment_status($eid);
		$data['status'] = $this->protogeni->sliver_status($eid);
		$this->load->view('sliver_status', $data);
	}

	function delete_sliver($eid=-1) {
		if ($eid > 0) var_dump($this->protogeni->delete_slice($eid));
		redirect($_SERVER['HTTP_REFERER'], 'refresh');
	}

	function status($eid=-1) {
		if ($eid < 0) redirect('/experiments', 'location', 302);
		$data['exp'] = $this->localmanager->experiment_status($eid);
		$data['status'] = $this->protogeni->sliver_status($eid);
		$this->load->view('experiment_status', $data);
	}

	function edit($eid=-1) {
		if ($eid < 0) redirect('/experiments', 'location', 302);
		if (isset($_POST['eid'])) {
			$edits = array(
					'name' => $_POST['name'],
					'rspec' => $_POST['rspec'],
			);
			$this->localmanager->update_experiment($_POST['eid'], $edits);
			redirect('/experiments/edit/' . $eid, 'location', 302);
		}
		$data['exp'] = $this->localmanager->get_experiment($eid);
		$this->load->view('experiment_edit', $data);
	}

}

/* End of file welcome.php */
/* Location: ./system/application/controllers/welcome.php */
