# configfileform

This project tries to provide a simple way to access
some config files over an HTTP server.

## The problem

When making use of configuration, the challenge is stay minimal.

This is (most of the time) in contrast with making it accessible
via a web server.

## The solution

The final goal is that configuration is stored in ascii
files like in /etc, as _KEY=VALUE_ pairs, and comments preceded
with _#_.

Using configfileform, you can easily create a .cgi script
like the one in examples/, that generates a web page, and
updates the file on request.
