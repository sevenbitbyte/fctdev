<?php

class User extends Controller {

	function User()
	{
		parent::Controller();	
		$this->output->set_header('Last-Modified: '.gmdate('D, d M Y H:i:s', time()).' GMT');
		$this->output->set_header('Expires: '.gmdate('D, d M Y H:i:s', time()).' GMT');
		$this->output->set_header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0, post-check=0, pre-check=0");
		$this->output->set_header("Pragma: no-cache");
		$this->usermanager->init();
	}
	
	function index()
	{
		redirect('/user/login', 'location', 302);
	}

	function login() {
		$view = "login";
		$data = array();
		if ($this->usermanager->current_user->logged) {
			$view = "user_info";
		} else if (!empty($_POST["username"]) && !empty($_POST['passphrase'])) {
			$res = $this->usermanager->login($_POST["username"], $_POST['passphrase']);
			if ($this->usermanager->current_user->logged) {
				$view = "user_info";
			} else {
				$view = "login";
				$data['error'] = $res;
			}
		}
		if ($view == "user_info") {
			redirect('/user/info', 'location', 302);
		} else {
			$this->load->view($view, $data);
		}
	}

	function info($action=Null) {
		if (!$this->usermanager->current_user->logged) {
			redirect('/user/login', 'location', 302);
		}
		if (isset($_FILES['certfile'])) {
			$this->usermanager->import_certificate($this->usermanager->current_user->username, $_FILES['certfile']);
		} else if ($action == "clear_cert") {
			$this->usermanager->clear_certificate($this->usermanager->current_username->username, $this->usermanager->current_username->certificate);
			redirect('/user/info', 'location', 302);
		}

		$data["user"] = $this->usermanager->current_user;
		$this->load->view("user_info", $data);
	}

	function logout() {
		if ($this->usermanager->current_user->logged) {
			$this->usermanager->logout();
		}
		redirect('/user/login', 'location', 302);
	}
}

/* End of file welcome.php */
/* Location: ./system/application/controllers/welcome.php */
