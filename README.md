# WebCpp2.0

WebCpp2.0 has global updates
1) FastCGI is over 100% faster compared to fastcgi library in Ubuntu 21.04 repository. This is achieved by buffer alignment 
and reducing the number of memory allocations.
2) Auto update when adding new static files without restart application.
3) Auto restart main application when it crashed(because of some mistakes).
Errors can appear due to various reasons, first of all, these are errors when processing url.

This release not contain
1) tracking url that called segfault or crash;
2) implementation of web logic, REST. You can implement API through boost::property_tree or flatbuffers code generator;
3) connecting with database.

Nginx settings like this
server { 
	listen 443 ssl;
	listen       80;
	server_name localhost; 
	ssl_certificate     /etc/ssl/certs/test-nginx-selfsigned.crt;
	ssl_certificate_key /etc/ssl/private/test-nginx-selfsigned.key;

	location / { 
		fastcgi_pass 127.0.0.1:8000;
		#fastcgi_pass  unix:/tmp/fastcgi/mysocket; 
		#fastcgi_pass localhost:9000; 
		 
		include fastcgi_params; 
		}
	}
