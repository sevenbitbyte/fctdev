#!/usr/bin/php
<?php

$nodes = array();
$links = array();

class Node {
	public $name = "";
	public $hardware = "";
	public $os = "";
	public $interfaces = array();
	public function __construct($n) {
		$this->name = $n;
	}
	public function add_interface($l, $i=NULL) {
		global $links;
		$interface = new Intfc($l, $this->name, $i);
		$this->interfaces[] = $interface;
		$interface->name = "if".array_search($interface, $this->interfaces);
		$links[$l]->add_raw_interface($interface);
	}
}

class Link {
	public $name = "";
	public $interfaces = array();
	public function __construct($n) {
		$this->name = $n;
	}
	public function add_raw_interface($if) {
		$this->interfaces[$if->node] = $if;
	}
	public function add_interface($n, $i=NULL) {
		$this->interfaces[$n] = new Intfc($this->name, $n, $i);
	}
}

class Intfc {
	public $name = "";
	public $node = "";
	public $ip = "";
	public function __construct($l, $n, $i=NULL) {
		$this->link = $l;
		$this->node = $n;
		$this->ip = $i;
	}
}

if ($argc == 1 || $argv[1] == "-") {
	$fh = fopen('php://stdin', 'r');
} else {
	$fh = fopen($argv[1], 'r');
}

while($line = fgets($fh)) {
	$matches = array();
	if (preg_match("/^set (node[0-9]+) /", $line, $matches)) {
		$nodes[$matches[1]] = new Node($matches[1]);
	} else if (preg_match('/^tb-set-hardware \$(node[0-9]+) ([a-zA-Z0-9]+)/', $line, $matches)) {
		$nodes[$matches[1]]->hardware = $matches[2];
	} else if (preg_match('/^tb-set-node-os \$(node[0-9]+) ([a-zA-Z0-9\-]+)/', $line, $matches)) {
		$nodes[$matches[1]]->os = $matches[2];
	} else if (preg_match('/^set (link[0-9]+) .* \$(node[0-9]+) \$(node[0-9]+)/', $line, $matches)) {
		$links[$matches[1]] = new Link($matches[1]);
		$nodes[$matches[2]]->add_interface($matches[1]);
		$nodes[$matches[3]]->add_interface($matches[1]);
	} else if (preg_match('/^tb-set-ip-link \$(node[0-9]+) \$(link[0-9]+) ([0-9]+.[0-9]+.[0-9]+.[0-9]+)/', $line, $matches)) {
		$links[$matches[2]]->interfaces[$matches[1]]->ip = $matches[3];
	}
}

fclose($fh);

echo '<?xml version="1.0" encoding="UTF-8"?>'."\n";
?>
<rspec xmlns="http://www.protogeni.net/resources/rspec/0.1"
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xsi:schemaLocation="http://www.protogeni.net/resources/rspec/0.1 http://www.protogeni.net/resources/rspec/0.1/request.xsd"
	type="request">
<?foreach ($nodes as $node) {?>
	<node virtual_id="<?=$node->name?>" virtualization_type="raw" exclusive="1" component_manager_uuid="urn:publicid:IDN+emulab.net+authority+cm">
		<node_type type_name="<?=$node->hardware?>" type_slots="1" />
<?foreach ($node->interfaces as $i=>$interface) { ?>
		<interface virtual_id="<?=$interface->name?>" />
<?}?>
	</node>
<?}?>
<?foreach ($links as $link) {?>
	<link virtual_id="<?=$link->name?>">
<?	foreach ($link->interfaces as $i=>$interface) {?>
		<interface_ref virtual_node_id="<?=$interface->node?>" virtual_interface_id="<?=$interface->name?>"<?if ($interface->ip != NULL) {?> tunnel_ip="<?=$interface->ip?>"<?}?> />
<?	}?>
	</link>
<?}?>
</rspec>
