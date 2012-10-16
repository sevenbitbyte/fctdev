<html>
<head>
<title>Experiment Status</title>
</head>
<body>
<h1><?=$exp['name']?></h1>
<strong>Status:</strong> <?=$status?><br>
<h3>Experiment Actions</h3>
<ul>
	<li><a href="<?=site_url('experiments/edit/'.$exp['eid'])?>">Edit experiment</a></li>
	<li><a href="<?=site_url('experiments/delete/'.$exp['eid'])?>">Delete Experiment</a></li>
</ul>
<h3>Slice/Sliver Actions</h3>
<ul>
	<li><a href="<?=site_url('experiments/create_sliver/'.$exp['eid'])?>">Create Sliver</a></li>
	<li><a href="<?=site_url('experiments/start_sliver/'.$exp['eid'])?>">Start Sliver</a></li>
	<li><a href="<?=site_url('experiments/sliver_status/'.$exp['eid'])?>">Sliver Status</a></li>
	<li><a href="<?=site_url('experiments/stop_sliver/'.$exp['eid'])?>">Stop Sliver</a></li>
	<li><a href="<?=site_url('experiments/delete_sliver/'.$exp['eid'])?>">Delete Sliver</a></li>
</ul>
</body>
</html>
