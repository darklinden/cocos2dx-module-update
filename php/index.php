<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>自动更新本地测试服务器文件管理</title>
<link rel="stylesheet" href="../styles.css">
</head>
<body>


<?PHP

	echo "<div class=\"card\">\n";
	echo "<p class=\"heading\">自动更新本地测试服务器文件管理</p>";
	echo "<button class=\"heading\" onclick=\"refreshFile()\">手动刷新</button>";
	echo "</div>";
	
    $folderPath = ".";
    if (!file_exists($folderPath)) {
     	mkdir($folderPath, 0777, true);
     	chmod($folderPath, 0777);
    }
    
	$refresh = $_GET["refresh"];
	if ($refresh) {
		$cmd = "/usr/bin/java -jar /usr/local/bin/jenkins-cli.jar -s http://127.0.0.1:8080 build \"fish-lua-module-update\"";
	
		echo "<p>正在执行: " . $cmd . "</p>";
	
		$cmd = $cmd . " 2>&1";
	
		exec($cmd, $retArr, $retVal);
 	
 		foreach ($retArr as $result) {
 			echo "<p>" . $result . "</p>";
 		}

		header("Location: index.php");
	}

	if ($_FILES["file"]) {
	
		if ($_FILES["file"]["error"] > 0)
		{
			echo "<p class=\"titlealert\">文件错误: " . $_FILES["file"]["error"] . "</p>";
		}
		else {
			if (file_exists($folderPath . $_FILES["file"]["name"]))
			{
				echo $_FILES["file"]["name"] . " already exists. ";
			}
			else
			{
				$desFile = $folderPath ."/". $_FILES["file"]["name"];
				move_uploaded_file($_FILES["file"]["tmp_name"], $desFile);
				chmod($desFile, 0777);
			}
		}
	}
	
	echo "</div>";
	
    // ou// tput file list in HTML TABLE format
    function getFileList($dir) {
        // array to hold return value
        $retval = array();
        // add trailing slash if missing
        if (substr($dir, -1) != "/") $dir .= "/";
        
        // open pointer to directory and read list of files
        $d = @dir($dir) or die("getFileList: Failed opening directory $dir for reading");
        
        while(false !== ($entry = $d->read())) {
            // skip hidden files
            if($entry[0] == ".") continue;
            if(is_dir("$dir$entry")) {
                $retval[] = array(
                                  "name" => "$dir$entry/",
                                  "type" => filetype("$dir$entry"),
                                  "size" => 0,
                                  "lastmod" => filemtime("$dir$entry")
                                  );
            } elseif(is_readable("$dir$entry")) {
                $retval[] = array(
                                  "name" => "$dir$entry",
                                  "type" => mime_content_type("$dir$entry"),
                                  "size" => filesize("$dir$entry"),
                                  "lastmod" => filemtime("$dir$entry")
                                  );
            }
        }
        
        $d->close();
        return $retval;
    }
    
    $dirlist = getFileList($folderPath);
    
    date_default_timezone_set('PRC');
    
    foreach ($dirlist as $file) {
    
    	if (basename($file['name']) == 'index.php') { continue; }
    
        echo "<div class=\"card\">\n";
        echo "<p class=\"text\"><a href=\"{$file['name']}\">", basename($file['name']), "</a></p>\n";
        echo "<p class=\"text\">最后修改时间: ", date("Y-m-d H:i:s", $file['lastmod']), "</p>";
        echo "</div>";
    }
    
?>
    
<script>

function refreshFile() {

	if (window.confirm('你确定发送刷新命令吗？')) {
		window.location.replace("index.php?refresh=yes");
		return true;
	}
}

</script>

</body>
</html>
