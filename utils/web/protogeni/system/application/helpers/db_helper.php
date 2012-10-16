<?php

function db_get() {
	static $db = Null;
	if ($db != Null) {
		return $db;
	}

	$CI = & get_instance();
	$db = & $CI->db;

	if ($db == Null) {
		trigger_error("No database! You *MUST* call load->database() first!", E_USER_ERROR);
	}

	return $db;
}

function db_create_table($name, $columns, $version=0) {
	$db = & db_get();
	$table_exists = $db->table_exists($name);
	if ($table_exists) {
		$res = $db->query("SELECT version FROM geni_tables WHERE name=?", array($name));
		if (!($res->num_rows() > 0 && $res->row()->version == $version)) {
			$res->free_result();
			$sql = "DROP TABLE " . $name;
			$db->simple_query($sql);
			$table_exists = false;
		} else {
			$res->free_result();
		}
	}
	if (!$table_exists) {
		$sql = "CREATE TABLE " . $name . " (" . $columns . ")";
		$db->simple_query($sql);
		$sql = "DELETE FROM geni_tables WHERE name='" . $name . "'";
		$db->simple_query($sql);
		$sql = "INSERT INTO geni_tables (name, version) VALUES ('" . $name . "', " . $version . ")";
		$db->simple_query($sql);
	}
}

function db_update_cache($name, $value=Null) {
	$db = & db_get();

	$sql = "DELETE FROM geni_cache WHERE name='" . $name . "'";
	if ($value !== Null)
		$sql .= " AND value='" . $value . "'";
	$db->simple_query($sql);

	$sql = "INSERT INTO geni_cache (last_updated, name";
	if ($value !== Null)
		$sql .= ", value";
	$sql .= ") VALUES (" . date("U") . ", '" . $name . "'";
	if ($value !== Null)
		$sql .= ", '" . $value . "'";
	$sql .= ")";
	$db->simple_query($sql);
}

function db_check_cache($name, $max_age, $value=Null) {
	$db = & db_get();

	$sql = "SELECT last_updated FROM geni_cache WHERE name='" . $name . "'";
	if ($value !== Null)
		$sql .= " AND value='" . $value . "'";

	$res = $db->query($sql);
	if ($res->num_rows() > 0) {
		$last_updated = $res->row()->last_updated;
	} else {
		$last_updated = -1;
	}
	$res->free_result();

	return ($last_updated > 0 && (date("U") - $last_updated < $max_age));
}
?>
