<html>
<head>
<title>Experiments</title>
<script type="text/javascript"  src="/js/jquery.js"></script>
<script type="text/javascript" >
	//$(document).ready();
</script>
</head>
<body>
Upload Experiment
<form action="<?=site_url('experiments/import')?>" method="post" enctype="multipart/form-data">
<input type="hidden" name="project" value="1">
<input type="text" name="name">
<input type="file" name="rspec">
<input type="submit">
</form>
<ul>
<?foreach ($experiments as $exp):?>
<li><a href="<?=site_url('experiments/status/'.$exp['eid'])?>"><?=$exp['name']?></a></li>
<?endforeach?>
</ul>
</body>
</html>
