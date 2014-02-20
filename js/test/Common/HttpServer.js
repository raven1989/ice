// **********************************************************************
//
// Copyright (c) 2003-2014 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

var http = require("http");
var https = require("https");
var url = require("url");
var crypto = require("crypto");
var fs = require("fs");
var path = require("path");

var httpProxy = null;
try
{
    httpProxy = require("http-proxy");
}
catch(e)
{
    console.warn("Warning: couldn't find http-proxy module, it's necessary to run the hello demo,\n" +
                 "you can run the following command to install it: npm install http-proxy\n");
}

var MimeTypes =
{
    css: "text/css",
    html: "text/html",
    ico: "image/x-icon",
    jpeg: "image/jpeg",
    jpg: "image/jpeg",
    js: "text/javascript",
    png: "image/png",
};

var FileServant = function(basePath)
{
    this._basePath = path.resolve(basePath);
};

FileServant.prototype.processRequest = function(req, res)
{
    var filePath = path.resolve(path.join(this._basePath, req.url.pathname));

    var ext = path.extname(filePath).slice(1);
    
    //
    // When the browser ask for a .js file and it has support for gzip content
    // check if a gzip version (.js.gz) of the file exists and use that instead.
    //
    if(ext == "js" && req.headers["accept-encoding"].indexOf("gzip") !== -1)
    {
        fs.stat(filePath + ".gz",
                function(err, stats)
                {
                    if(err || !stats.isFile())
                    {
                        fs.stat(filePath,
                                function(err, stats)
                                {
                                    doRequest(err, stats, filePath);
                                });
                    }
                    else
                    {
                        doRequest(err, stats, filePath + ".gz");
                    }
                });
    }
    else
    {
        fs.stat(filePath,
                    function(err, stats)
                    {
                        doRequest(err, stats, filePath);
                    });
    }
    
    var doRequest = function(err, stats, filePath)
    {
        if(err)
        {
            if(err.code === "ENOENT")
            {
                res.writeHead(404);
                res.end("404 Page Not Found");
                console.log("HTTP/404 (Page Not Found) " + req.method + " " + req.url.pathname);
            }
            else
            {
                res.writeHead(500);
                res.end("500 Internal Server Error");
                console.log("HTTP/500 (Internal Server Error) " + req.method + " " + req.url.pathname);
            }
        }
        else
        {
            if(!stats.isFile())
            {
                res.writeHead(403);
                res.end("403 Forbiden");
                console.log("HTTP/403 (Forbiden) " + req.method + " " + req.url.pathname);
            }
            else
            {
                //
                // Create a md5 using the stats attributes
                // to be used as Etag header.
                //
                var hash = crypto.createHash("md5");
                hash.update(stats.ino.toString());
                hash.update(stats.mtime.toString());
                hash.update(stats.size.toString());
                
                var headers = 
                {
                    "Content-Type": MimeTypes[ext] || "text/plain",
                    "Content-Length": stats.size,
                    "Last-Modified": new Date(stats.mtime).toUTCString(),
                    "Etag": hash.digest("hex")
                };
                
                if(path.extname(filePath).slice(1) == "gz")
                {
                    headers["Content-Encoding"] = "gzip";
                }
                
                //
                // Check for conditional request headers, if-modified-since
                // and if-none-match.
                //
                var modified = true;
                if(Date.parse(req.headers["if-modified-since"]) == stats.mtime.getTime())
                {
                    modified = false;
                }
                else if(req.headers["if-none-match"] !== undefined)
                {
                    modified = req.headers["if-none-match"].split(" ").every(
                        function(element, index, array)
                        {
                            return element !== headers["Etag"];
                        });
                }

                //
                // Not Modified
                //
                if(!modified)
                {
                    res.writeHead(304, headers);
                    res.end();
                    console.log("HTTP/304 (Not Modified) " + req.method + " " + req.url.pathname);
                }
                else
                {
                    res.writeHead(200, headers);
                    if(req.method === "HEAD")
                    {
                        res.end();
                    }
                    else
                    {
                        fs.createReadStream(filePath, { "bufferSize": 4 * 1024 }).pipe(res);
                    }
                    console.log("HTTP/200 (Ok) " + req.method + " " + req.url.pathname);
                }
            }
        }
    }
};

var HttpServer = function(host, ports)
{
    this._host = host;
    this._ports = ports;
};

HttpServer.prototype.start = function()
{
    var baseDir = path.join(__dirname, "../../../certs/wss");
    var options = {
        passphrase: "password",
        key: fs.readFileSync(path.join(baseDir, "s_rsa1024_priv.pem")),
        cert: fs.readFileSync(path.join(baseDir, "s_rsa1024_pub.pem"))
    };
    
    httpServer = http.createServer();
    httpsServer = https.createServer(options);

    if(httpProxy)
    {
        var proxyWS = httpProxy.createProxyServer({ 
            target : "http://localhost:10002",
            secure : false,
        });
        var proxyWSS = httpProxy.createProxyServer({
            target : "https://localhost:10003",
            secure : false,
        });
    }
    
    var self = this;
    [httpServer, httpsServer].forEach(function(server)
                    {
                        server.on("request", function(req, res)
                                  {
                                      self.processRequest(req, res);
                                  });
                    });

    if(httpProxy)
    {
        httpServer.on("upgrade", function(req, socket, head)
                      {
                          if(req.url == "/demows")
                          {
                              proxyWS.ws(req, socket, head, function (err) 
                                         {
                                             socket.end();
                                         });
                          }
                          else
                          {
                              socket.end();
                          }
                      });
        httpsServer.on("upgrade", function(req, socket, head)
                       {
                           var errCB = function (err) 
                           {
                               socket.end();
                           }

                           if(req.url == "/demows")
                           {
                               proxyWS.ws(req, socket, head, errCB);
                           }
                           else if(req.url == "/demowss")
                           {
                               proxyWSS.ws(req, socket, head, errCB);
                           }
                           else
                           {
                               socket.end();
                           }
                       });
    }

    httpServer.listen(8080, this._host);
    httpsServer.listen(9090, this._host);
    console.log("listening on ports 8080 (http) and 9090 (https)...");
};

var defaultController = new FileServant(path.join(__dirname, "../.."));

HttpServer.prototype.processRequest = function(req, res)
{
    var self = this;
    
    var dataCB = function(data)
    {
    };
    
    var endCB = function()
    {
        req.url = url.parse(req.url);
        defaultController.processRequest(req, res);
    };
    
    req.on("data", dataCB);
    req.on("end", endCB);
};

var server = new HttpServer("0.0.0.0", [8080, 9090]);
server.start();
