<html>
<head>
<title>Login</title>
<script src="/js/jquery.js"></script>
<script>
	//$(document).ready();
</script>
</head>
<body>
<form action="<?=$_SERVER['PHP_SELF']?>" method="post">
<label for="uname">Username:</label><input type="text" name="username" id="uname"<?=(isset($username)?' value="'.$username.'"':'')?>><br>
<label for="pphrase">Passphrase:</label><input type="password" name="passphrase" id="pphrase"><br>
<input type="submit">
</form>
<?if (isset($error)) {
echo "<div><em>Error:</em> ".$error."</div>";
}?>
</body>
</html>
