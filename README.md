# zswalc

Zscheile Web Application Light Chat is an highly insecure,
unreliable, experimental ~~ fastcgi ~~ HTTP-based chatting app,
which utilizes javascript to update the chat view.

## expected command line arguments
```
USAGE:
    zswalc [OPTIONS] --database <database> --serv-addr <serv–addr>

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

OPTIONS:
    -b, --database <database>      sets the file where to store the chat data
    -a, --serv-addr <serv–addr>    sets the server address to bind/listen to
    -r, --vroot <vroot>            sets the HTTP base path of this app (defaults to '')
```

## lighttpd example
```
###############################################################################
# zswebapp.conf
# include'd by lighttpd.conf.
###############################################################################

$HTTP["url"] =~ "^/zschat($|/)" {
  proxy.server = ( "" => (( "host" => "127.0.0.1", "port" = "9003" )) )
}

# vim: set ft=conf foldmethod=marker et :
```
