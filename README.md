# `$.Sparc(c++1y); // lightweight web framework `

Sparc C++1y is Linux lightweight [Spark](http://sparkjava.com/) web framework  for designing web applications/API
in morden C++. The framework uses the blazing fast components of [kore](https://kore.io/).

####*This project is still under heavy development*

### Getting Started

```
#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);
    
    get("/hello", $(req, res) {
        res.body("Hello World");
        return OK;
    });
    return $exit();
}

```
