idf_component_register(
    SRCS 
        "source/core_http_client.c"
        "source/dependency/3rdparty/llhttp/src/api.c"
        "source/dependency/3rdparty/llhttp/src/http.c"
        "source/dependency/3rdparty/llhttp/src/llhttp.c"
    INCLUDE_DIRS 
        "source/include"
        "source/interface"
        "source/dependency/3rdparty/llhttp/include"
    REQUIRES lwip main
)