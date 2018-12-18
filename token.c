/* standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* json-c (https://github.com/json-c/json-c) */
#include <json-c/json.h>

/* libcurl (http://curl.haxx.se/libcurl/c) */
#include <curl/curl.h>

/* holder for curl fetch */
struct curl_fetch_st {
    char *payload;
    size_t size;
};

/* callback for curl fetch */
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                             /* calculate buffer size */
    struct curl_fetch_st *p = (struct curl_fetch_st *) userp;   /* cast pointer to fetch struct */

    //printf("## ##\n");
    /* expand buffer */
    p->payload = (char *) realloc(p->payload, p->size + realsize + 1);

    /* check buffer */
    if (p->payload == NULL) {
      /* this isn't good */
      fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
      /* free buffer */
      free(p->payload);
      /* return */
      return -1;
    }

    /* copy contents to buffer */
    memcpy(&(p->payload[p->size]), contents, realsize);

    /* set new buffer size */
    p->size += realsize;

    /* ensure null termination */
    p->payload[p->size] = 0;

    //printf("## %s\n", p->payload);

    /* return size */
    return realsize;
}

/* fetch and return url body via curl */
CURLcode curl_fetch_url(CURL *ch, const char *url, struct curl_fetch_st *fetch) {
    CURLcode rcode;                   /* curl result code */

    /* init payload */
    fetch->payload = (char *) calloc(1, sizeof(fetch->payload));

    /* check payload */
    if (fetch->payload == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to allocate payload in curl_fetch_url");
        /* return error */
        return CURLE_FAILED_INIT;
    }

    /* init size */
    fetch->size = 0;

    /* set url to fetch */
    curl_easy_setopt(ch, CURLOPT_URL, url);

    /* set calback function */
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);

    /* pass fetch struct pointer */
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) fetch);

    /* set default user agent */
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* set timeout */
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, 5);

    /* enable location redirects */
    //curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

    /* set maximum allowed redirects */
    //curl_easy_setopt(ch, CURLOPT_MAXREDIRS, 1);

    /* fetch the url */
    rcode = curl_easy_perform(ch);

    /* return */
    return rcode;
}

int main(int argc, char *argv[]) {
    CURL *ch;                                               /* curl handle */
    CURLcode rcode;                                         /* curl result code */
    long response_code;
    char *location;
    char system_cmd[1024];

    json_object *json;                                      /* json post body */
    json_object *json_pia;                                  /* json post body */
    json_object *json_all;                                  /* json post body */
    json_object *json_sd;                                   /* json post body */

    enum json_tokener_error jerr = json_tokener_success;    /* json parse error */

    struct curl_fetch_st curl_fetch;                        /* curl fetch struct */
    struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
    struct curl_slist *headers = NULL;                      /* http headers to send with request */

#if 0
    /* url to test site */
    char *url_format = "https://www.amazon.com/ap/oa?scope=alexa%3Aall&redirect_uri=%s&response_type=code&client_id=%s&scope_data=%7B%22alexa%3Aall%22%3A+%7B%22productInstanceAttributes%22%3A+%7B%22deviceSerialNumber%22%3A+%22%s%22%7D%2C+%22productID%22%3A+%22%s%22%7D%7D";
    char *redirect_uri = "https://192.168.1.223:5001";
    char *client_id    = "amzn1.application-oa2-client.3cf73e54dd0d4bfdb0584b28480cd609";
    char *serial_num   = "001";
    char *product_id   = "jonathan_avs_device";
    //secret = c1c2ff1b1e9b735d2dd73daaa6e00d3578762373b86a701f6612440e609bb8e8
#else
    char *url_format = "https://www.amazon.com/ap/oa?scope=alexa%3Aall&redirect_uri=%s&response_type=code&client_id=%s&scope_data=%7B%22alexa%3Aall%22%3A+%7B%22productInstanceAttributes%22%3A+%7B%22deviceSerialNumber%22%3A+%22%s%22%7D%2C+%22productID%22%3A+%22%s%22%7D%7D";
    char *redirect_uri = "http://192.168.1.223:5001/code";
    char *client_id    = "amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580";
    char *serial_num   = "001";
    char *product_id   = "RaspberryEcho";
#endif
    //char url[1024];
    //snprintf(url, 1024, url_format, redirect_uri, client_id, serial_num, product_id);

    //char *url = "https://www.amazon.com/ap/oa?scope=alexa%3Aall&redirect_uri=http%3A%2F%2F192.168.100.242%3A5000%2Fcode&response_type=token&client_id=amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580&scope_data=%7B%22alexa%3Aall%22%3A+%7B%22productInstanceAttributes%22%3A+%7B%22deviceSerialNumber%22%3A+%22001%22%7D%2C+%22productID%22%3A+%22RaspberryEcho%22%7D%7D";
#if 0
    char *url = "https://www.amazon.com/ap/oa?" \
					"client_id=amzn1.application-oa2-client.c01edcaad70d4a9c9f87119877ef6a2b&" \
					"scope=alexa%3Aall&" \
					"scope_data=%7B%22" \
						"alexa%3Aall%22%3A+%7B%22"
							"productID%22%3A+%22compal_avs_demo%22%7D%7D" \
							"productInstanceAttributes%22%3A+%7B%22" \
								"deviceSerialNumber%22%3A+%22001%22%7D%2C+%22&" \
					"response_type=code&" \
					"redirect_uri=https%3A%2F%2F192.168.1.232%3A5001";
#else
    char *url = "https://www.amazon.com/ap/oa?" \
					"client_id=amzn1.application-oa2-client.9ee71077c9d142a7ac629fdb611d5795&" \
					"scope=alexa%3Aall&" \
					"scope_data=%7B%22" \
						"alexa%3Aall%22%3A+%7B%22"
							"productID%22%3A+%22DMService%22%7D%7D" \
							"productInstanceAttributes%22%3A+%7B%22" \
								"deviceSerialNumber%22%3A+%22A53VYO51249JG%22%7D%2C+%22&" \
					"response_type=token&" \
					"state=6042d10f-6bcd-49&" \
					"redirect_uri=https%3A%2F%2F192.168.1.1%3A5001";
#endif

				/* Implicit Grant */
				//https://www.amazon.com/ap/oa?
				//	client_id=amzn1.application-oa2-client.b91a4d2fd2f641f2a15ea469&
				//	scope=alexa%3Aall&
				//	scope_data=%7B%22
				//		alexa%3Aall%22%3A%7B%22
				//			productID%22%3A%22Speaker%22%2C%22
				//			productInstanceAttributes%22%3A%7B%22
				//				deviceSerialNumber%22%3A%2212345%22%7D%7D%7D&
				//	response_type=token&
				//	state=6042d10f-6bcd-49&
				//	redirect_uri=https%3A%2F%2Flocalhost

				/* Authorization Code Grant */
				//https://www.amazon.com/ap/oa?
				//	client_id=amzn1.application-oa2-client.b91a4d2fd2f641f2a15ea469&
				//	scope=alexa%3Aall&
				//	scope_data=%7B%22
				//		alexa%3Aall%22%3A%7B%22
				//			productID%22%3A%22Speaker%22%2C%22
				//			productInstanceAttributes%22%3A%7B%22
				//				deviceSerialNumber%22%3A%2212345%22%7D%7D%7D&
				//	response_type=code&
				//	state=6042d10f-6bcd-49&
				//	redirect_uri=https%3A%2F%2Flocalhost

    
	printf("REQUEST URL = %s\n", url);

    /* init curl handle */
    if ((ch = curl_easy_init()) == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");
        /* return error */
        return 1;
    }

    

    /*
    GET https://www.amazon.com/ap/oa?
    scope=alexa%3Aall&
    redirect_uri=http%3A%2F%2F192.168.100.242%3A5000%2Fcode&
    response_type=code&
    client_id=amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580&
    scope_data=%7B%22alexa%3Aall%22%3A+%7B%22
      productInstanceAttributes%22%3A+%7B%22
        deviceSerialNumber%22%3A+%22001%22%7D%2C+%22
      productID%22%3A+%22RaspberryEcho%22%7D%7D
    {
      'scope': 'alexa:all', 
      'redirect_uri': 'http://192.168.100.242:5000/code', 
      'response_type': 'code', 
      'client_id': 'amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580', 
      'scope_data': '
       {
         "alexa:all": 
         {
           "productInstanceAttributes": 
           {"deviceSerialNumber": "001"}, 
           "productID": "RaspberryEcho"
         }
      }'
    }
    { "scope": "alexa:all", 
      "redirect_uri": "http:\/\/192.168.100.242:5000\/code", 
      "response_type": "code", 
      "client_id": "amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580", 
      "scope_data": { 
          "alexa:all": 
          { 
            "productInstanceAttributes": 
            { "deviceSerialNumber": "001" }, 
            "productID": "RaspberryEcho" 
          } 
      } 
    }
    {
      'redirect_uri': 'http://192.168.100.242:5000/code',
      'client_secret': '15e008b14c093988a438ba6f639c9889440edb2710f7de2e223b8269a726c13d',
      'code': u'ANCrnLCanPDzRKDGfZnD',
      'client_id': 'amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580',
      'grant_type': 'authorization_code'
    }
    */

    /* set content type */
    //headers = curl_slist_append(headers, "Accept: application/json");
    //headers = curl_slist_append(headers, "Content-Type: application/json");

#if 0
    /* create json object for post */
    json     = json_object_new_object();
    json_pia = json_object_new_object();
    json_all = json_object_new_object();
    json_sd  = json_object_new_object();

    json_object_object_add(json_pia, "deviceSerialNumber",        json_object_new_string("001"));
    json_object_object_add(json_all, "productInstanceAttributes", json_pia);
    json_object_object_add(json_all, "productID",                 json_object_new_string("RaspberryEcho"));
    json_object_object_add(json_sd,  "alexa:all",                 json_all);
    json_object_object_add(json,     "scope",                     json_object_new_string("alexa:all"));
    //json_object_object_add(json,     "redirect_uri",  json_object_new_string("http://192.168.100.242:5001"));
    json_object_object_add(json,     "redirect_uri",  json_object_new_string("http://192.168.1.1:5001"));
    json_object_object_add(json,     "response_type", json_object_new_string("code"));
    json_object_object_add(json,     "client_id",     json_object_new_string("amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580"));
    json_object_object_add(json,     "scope_data",                json_sd);
    json_object_object_add(json, "redirect_uri",  json_object_new_string("http://localhost:5000/token"));
    json_object_object_add(json, "client_secret", json_object_new_string("15e008b14c093988a438ba6f639c9889440edb2710f7de2e223b8269a726c13d"));
    json_object_object_add(json, "code",          json_object_new_string("ANCrnLCanPDzRKDGfZnD"));
    json_object_object_add(json, "client_id",     json_object_new_string("amzn1.application-oa2-client.0dfdb3ed874b4451bfc5b62376473580"));
    json_object_object_add(json, "grant_type",    json_object_new_string("authorization_code"));
#endif

    /* set curl options */
    //curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0L);
    //curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    //curl_easy_setopt(ch, CURLOPT_POSTFIELDS, json_object_to_json_string(json));
    curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(ch, CURLOPT_VERBOSE, 1);

    /* fetch page and capture return code */
    curl_easy_setopt(ch, CURLOPT_URL, url);
    rcode = curl_fetch_url(ch, url , cf);
    //rcode = curl_easy_perform(ch);

    /* Check for errors */ 
    if (rcode != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(rcode));
    else {
      rcode = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &response_code);
      if ((rcode == CURLE_OK) &&
         ((response_code / 100) != 3)) {
        /* a redirect implies a 3xx response code */ 
        fprintf(stderr, "Not a redirect(response = %ld).\n", response_code);
      }
      else {
        rcode = curl_easy_getinfo(ch, CURLINFO_REDIRECT_URL, &location);
 
        if ((rcode == CURLE_OK) && location) {
          /* This is the new absolute URL that you could redirect to, even if
           * the Location: response header may have been a relative URL. */ 
          printf("Redirected to: %s\n", location);
          snprintf(system_cmd, 1024, "browse %s", location);
          system(system_cmd);
        }
      }
    }

    /* cleanup curl handle */
    curl_easy_cleanup(ch);

    /* free headers */
    curl_slist_free_all(headers);

    /* free json object */
    json_object_put(json);

    /* check return code */
    if (rcode != CURLE_OK || cf->size < 1) {
        /* log error */
        //fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s",
        //    url, curl_easy_strerror(rcode));
        /* return error */
        return 2;
    }

    /* check payload */
    if (cf->payload != NULL) {
        /* print result */
        //printf("CURL Returned: \n%s\n", cf->payload);
        /* parse return */
        /* free payload */
        free(cf->payload);
    } else {
        /* error */
        fprintf(stderr, "ERROR: Failed to populate payload");
        /* free payload */
        free(cf->payload);
        /* return */
        return 3;
    }

    /* check error */
    if (jerr != json_tokener_success) {
        /* error */
        fprintf(stderr, "ERROR: Failed to parse json string");
        /* free json object */
        json_object_put(json);
        /* return */
        return 4;
    }

    /* debugging */
    //printf("Parsed JSON: %s\n", json_object_to_json_string(json));

    /* exit */
    return 0;
}
