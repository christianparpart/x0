Flow Control Configuration Language
===================================

```
handler main {
  if req.path =^ '/private/' {
    return 403;
  }
  docroot '/var/www';
  staticfile;
}
```
