
<!DOCTYPE html>
<meta charset="utf-8">
<head><title>index</title></head>
<link rel="stylesheet" href="//code.jquery.com/ui/1.11.4/themes/smoothness/jquery-ui.css">
<link href= "../css/main.css" rel="stylesheet" type ="text/css">
<script src="//code.jquery.com/jquery-1.10.2.js"></script>
<script src="//code.jquery.com/ui/1.11.4/jquery-ui.js"></script>


<header>
<ul class = "list">
<li>
<a href = "http://localhost/sense/data/index.php" style="text-decoration: none">Data</a>
</li>
<li>
<a href = "http://localhost/sense/settings/details.html" style="text-decoration: none">Settings</a>
</li>
</ul>
<h1><a href="http://localhost/sense/data/index.php" style="text-decoration: none">/ tempR /</a></h1>
</header>
<body>

<nav>
<p>Data</p><br>

<a href="http://localhost/sense/data/index.php" style="text-decoration: none">Overview</a><br>
<a href="http://localhost/sense/data/graphs.php" style="text-decoration: none">Graphs</a><br>
<a href="http://localhost/sense/data/analytics.html" style="text-decoration: none">Analytics</a><br>
<a href="http://localhost/sense/data/alarms.html" style="text-decoration: none">Alarms</a></nav>
<!--highlight the current section of the navigation bar-->
<script>
$(function(){
  $('a').each(function() {
    if ($(this).prop('href') == window.location.href) {
      $(this).addClass('current');
    }
  });
});
</script>
<section>
<h1></h1>

</section>


</body>