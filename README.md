Flow Control Configuration Language
===================================

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/libflow.png) ](http://travis-ci.org/xzero/libflow)

```
handler main {
  if req.path =^ '/private/' {
    return 403;
  }
  docroot '/var/www';
  staticfile;
}
```
