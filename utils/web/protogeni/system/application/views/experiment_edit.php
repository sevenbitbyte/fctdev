<html>
<head>
<title>Experiment Edit</title>
<script src="/js/jquery.js"></script>
<script>
	//$(document).ready();
</script>
</head>
<body>
<h1><?=$exp['name']?></h1>
<form method="post" action="<?=$_SERVER['PHP_SELF']?>" enctype="multipart/form-data">
<input type="hidden" name="eid" value="<?=$exp['eid']?>">
<label for="name">Name:</label><br>
<input type="text" name="name" value="<?=$exp['name']?>"><br>
<label for="rspec">RSPEC:</label><br>
<textarea name="rspec" rows="20" cols="100" wrap="off"><?=$exp['rspec']?></textarea><br>
<input type="submit" value="Save"><input type="button" value="Close" onclick="document.location.href='<?=site_url("/experiments/status/".$exp['eid'])?>'; return false;">
</form>
<br>
</body>
</html>
