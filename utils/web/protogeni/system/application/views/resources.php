<style>
.toggle_state {
	font-style: Fixed, monospace;
	text-decoration: none;
}
.toggle {
	display: block;
}
</style>
<?
if (empty($resources))
	trigger_error("Error loading resource information for $cm_url!", E_USER_ERROR);
if ($raw_xml) {
	echo "<PRE>";
	echo htmlentities($resources);
	echo "</PRE>";
} else {
	foreach ($resources->nodes as $node) {?>
<label class="toggle">Node: <?=$node->name?></label>
<ul style="display: none">
<?
foreach ($node as $key => $val) {
	if ($key == "types") {
		foreach ($val as $type) {?>
<li>Type: <?=$type->name?> [<?=$type->slots?>]</li>
<?		}
	} else if ($key == "ifaces") {
		foreach ($val as $iface) {?>
<li>Interface: <?=$iface->short_id?> (<?=$iface->role?>)</li>
<?		}
	} else if (is_array($val)) {
		foreach ($val as $element) {?>
<li><?=$key?>: <?=print_r($element)?></li>
<?
		}
	} else {?>
	<li><?=$key?>: <?=$val?></li>
<?			}
			}?>
</ul>
<?	}
	foreach ($resources->links as $link) {?>
<span class="toggle">Link: <?=$link->name?></span>
<ul style="display: none">
<?
foreach ($link as $key => $val) {
	if ($key == "link_types") {
		foreach ($val as $type) {?>
<li>Link_type: <?=$type->name?></li>
<?		}
	} else if ($key == "iface_refs") {
		foreach ($val as $iface) {?>
<li>Interface_Ref: <?=$iface->node_iface_id?></li>
<?		}
	} else if (is_array($val)) {
		foreach ($val as $element) {?>
<li><?=$key?>: <?=print_r($element)?></li>
<?
		}
	} else {?>
	<li><?=$key?>: <?=$val?></li>
<?			}
			}?>
</ul>
<?	}
}?>
<script>
function toggle(obj) {
	if ($(obj).data("open") == true) {
		$(obj).parent().next().hide();
		$(obj).html("+");
		$(obj).data("open", false);
	} else {
		$(obj).parent().next().show();
		$(obj).html("-");
		$(obj).data("open", true);
	}
}
$('.toggle').append(' <a href="#" class="toggle_state">+</a>'); 
$('.toggle_state').click(function () { toggle(this); return false; });
$('.toggle_state').data("open", false);
</script>
