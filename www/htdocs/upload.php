
<form enctype="multipart/form-data" action="upload.php" method="POST">
  Choose a file to upload: <input name="uploadedfile" type="file" /><br />
  <input type="submit" value="Upload File" />
</form>

<?php
  if (isset($_FILES['uploadedfile']) && $_FILES['uploadedfile']) {
    // Where the file is going to be placed 
    $target_path = "/tmp/";

    /* Add the original filename to our target path.  
       Result is "uploads/filename.extension" */
    $target_path = $target_path . basename( $_FILES['uploadedfile']['name']); 

    if (move_uploaded_file($_FILES['uploadedfile']['tmp_name'], $target_path)) {
        echo "The file ".  basename( $_FILES['uploadedfile']['name']). 
            " has been uploaded";
    } else{
        echo "There was an error uploading the file, please try again! (" . $target_path . ')';
    }
  }
?>
