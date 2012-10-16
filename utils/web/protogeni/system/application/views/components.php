<html>
<head>
<title>Component Managers</title>
<script src="/js/jquery.js"></script>
<script>
	function get_view(obj, uri, params, timeout) {
		if (timeout == null)
			timeout = 30000;
		$(obj).html('<em>Loading...</em>');
		$.ajax({
			url: '/protogeni/'+uri,
			type: 'POST',
			data: params,
			dataType: 'html',
			timeout: timeout,
			success: function (html) {
					$(obj).html(html);
			}
		});
	}
	$(document).ready(function() {
		$.ajax({
			url: '/protogeni/components/list_cms/',
			type: 'POST',
			data: null,
			dataType: 'json',
			timeout: 30000,
			error: function (req, err) {
				alert(err + ": " + req);
			},
			success: function (json) {
				var html = '<option value="">Select a CM to see available resources</option>';
				for (var c in json.components) {
					var component = json.components[c];
					html += '<option value="'+component.url+'">'+component.urn+'</option>';
				}
				$('select#cm').html(html);
				$('#choose_cm').removeAttr('disabled');
			}
		});
	});
	function show_resources(url) {
		if (url == '')
			return false;
		$('#resources').html('Loading resources for '+url+'...');
		$.ajax({
			url: '/protogeni/components/discover_resources/',
			type: 'POST',
			data: {'url': url},
			dataType: 'html',
			timeout: 30000,
			error: function (req, err) {
				$('#resources').html(err + ": " + req);
			},
			success: function (json) {
				$('#resources').html(json);
			}
		});
	}
</script>
</head>
<body>
<form>
<select name="cm" id="cm">
<option>Loading CMs...</option>
</select>
<input type="button" id="choose_cm" onclick="show_resources($('select#cm').val()); return false;" value="Go" disabled>
</form>
<div id="resources">
</div>
</body>
</html>
