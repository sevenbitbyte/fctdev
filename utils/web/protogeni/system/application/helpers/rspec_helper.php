<?
class RSPEC {
	var $type = "";
	var $generated = "";
	var $valid_until = "";
	var $nodes = array();
	var $links = array();
}
class Node {
	var $cm_uuid = "";
	var $name = "";
	var $uuid = "";
	var $available = False;
	var $exclusive = False;
	var $interfaces = array();
	var $types = array();
}
class Iface {
	var $id = "";
	var $short_id = "";
	var $role = "";
	var $public_ipv4 = "";
}
class Node_Type {
	var $name = "";
	var $slots = "";
}
class Link {
	var $cm_uuid = "";
	var $name = "";
	var $uuid = "";
	var $bandwidth = 0;
	var $latency = 0;
	var $packet_loss = 0;
 	var $iface_refs = array();
	var $link_types = array();
}
class Iface_Ref {
	var $node_uuid = "";
	var $node_iface_id = "";
}
class Link_Type {
	var $name = "";
}
function parse_rspec($xml) {
	$root = new SimpleXMLElement($xml);
	$rspec = new RSPEC();
	$rspec->type = (string)$root['type'];
	$rspec->generated = (string)$root['generated'];
	$rspec->valid_until = (string)$root['valid_until'];
	foreach ($root->children() as $component) {
		switch ($component->getName()) {
			case "node":
				$node = new Node();
				$node->name = (string)urldecode($component['component_name']);
				$node->uuid = (string)urldecode($component['component_uuid']);
				$node->cm_uuid = (string)$component['component_manager_uuid'];
				foreach ($component->children() as $attr) {
					switch($attr->getName()) {
						case "node_type":
							$type = new Node_Type();
							$type->name = (string)$attr['type_name'];
							$type->slots = (int)$attr['type_slots'];
							$node->types[] = $type;
							break;
						case "available":
							$node->available = (bool)$attr[0];
							break;
						case "exclusive":
							$node->exclusive = (bool)$attr[0];
							break;
						case "interface":
							$iface = new Iface();
							$iface->id = (string)urldecode($attr['component_id']);
							$iface->short_id = substr($iface->id, strrpos($iface->id, ":")+1);
							$iface->role = (string)$attr['role'];
							$iface->public_ipv4 = (string)$attr['public_ipv4'];
							$node->ifaces[] = $iface;
							break;
					}
				}
				$rspec->nodes[] = $node;
				break;
			case "link":
				$link = new Link();
				$link->name = (string)$component['component_name'];
				$link->uuid = (string)urldecode($component['component_uuid']);
				$link->cm_uuid = (string)$component['component_manager_uuid'];
				foreach ($component->children() as $attr) {
					switch($attr->getName()) {
						case "node_type":
							$type = new Node_Type();
							$type->name = (string)$attr['type_name'];
							$type->slots = (int)$attr['type_slots'];
							$node->types[] = $type;
							break;
						case "bandwidth":
							$node->bandwidth = (int)$attr[0];
							break;
						case "latency":
							$node->latency = (int)$attr[0];
							break;
						case "packet_loss":
							$node->packet_loss = (int)$attr[0];
							break;
						case "interface_ref":
							$iface = new Iface_Ref();
							$iface->node_uuid = (string)urldecode($attr['component_node_uuid']);
							$iface->node_iface_id = (string)urldecode($attr['component_interface_id']);
							$link->iface_refs[] = $iface;
							break;
						case "link_type":
							$link_type = new Link_Type();
							$link_type->name = (string)$attr['type_name'];
							$link->link_types[] = $link_type;
							break;
					}
				}
				$rspec->links[] = $link;
				break;
		}
	}
	return $rspec;
}
?>
