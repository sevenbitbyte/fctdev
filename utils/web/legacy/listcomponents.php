#!/usr/bin/php
<?
include_once("Protogeni.php");
$GENI = new Protogeni();
#$params['credential'] = $GENI->get_self_credential();
#$response = $GENI->do_method("ch", "ListComponents", $params);
#$components = $response['value'];
$components = $GENI->list_components();
foreach ($components as $component) {
 echo $component['urn'].': '.$component['url']."\n";
}?>
