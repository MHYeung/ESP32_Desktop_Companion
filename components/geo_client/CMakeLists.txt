idf_component_register(
  SRCS "geo_client.c"
  INCLUDE_DIRS "include"
  REQUIRES esp_http_client json mbedtls wifi_time
)
