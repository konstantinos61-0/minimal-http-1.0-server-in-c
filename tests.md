### Manual Test Cases 
The test cases below were used during and after development to verify that the server functions as intended. They include:
- URLs for testing inside browsers
- Raw text requests that can be tested using telnet or similar tools.

All tests were performed locally on a machine running Ubuntu Linux.

Execute the chmod command, `chmod 000 letter/*` before testing in order for the 403 response to be triggered for the letter directory requests. 

#### URLs
Replace `8080` with the actual port the server is running on.  
```
http://localhost:8080/                      // 200 OK (serves /index.html)  
http://localhost:8080/page.html             // 200 OK (serves /page.html)  
http://localhost:8080/another_page.html     // 404 Not Found  
http://localhost:8080/search                // 301 Moved Permanently, Location: /search/  
http://localhost:8080/search/               // 200 OK (serves /search/index.html)  
http://localhost:8080/letter/               // 403 Forbidden 
```

#### Raw Text Requests
To test these requests run `telnet localhost 8080` (or the actual port). Then copy paste any of them into the telnet prompt. 
Telnet requests must terminate with an empty line (\r\n\r\n)

**Incomplete Requests**
```
GE                              // 400 Bad Request

GET /                           // 400 Bad Request

GET / HTTP                      // 400 Bad Request
```

**Method Testing**
```
POST / HTTP/1.0                 // 501 Not Implemented

HEAD / HTTP/1.0                 // 501 Not Implemented

GE / HTTP/1.0                   // 400 Bad Request

GETTING / HTTP/1.0              // 400 Bad Request
```

**URI Testing**
```
GET page.html HTTP/1.0         // 400 Bad Request


GET /path/over/256/bytes HTTP/1.0        // 400 Bad Request (replace the path with an actually long one)


GET /../ HTTP/1.0               // 403 Forbidden
```

**Version Testing**
```
GET / HTTP/2.0                  // 400 Bad Request
```

**Header Testing**
```
GET / HTTP/1.0
foo-header: foo-value
bar-header: bar-value           // 200 OK (serves /index.html)

GET / HTTP/1.0
foo-header: foo-value
bar header: bar value           // 400 Bad Request

GET / HTTP/1.0
foo-header: foo-value
bar-header: bar-value. This value is very long!! Random characters follow, proceed with caution...asdu9asdhasdasdasdasdasdasdasdasdasdasdasdasdasdsaddddasdasdasygasduioaegaiuwegaiusdgoawy8dgw7aasdasdasdasdasdasdasdasdasdasdasdasdasdasdasd8dasdasdwdfwadfsadasdasdsghadawdwadasaf
                                // 400 Bad Request
```
