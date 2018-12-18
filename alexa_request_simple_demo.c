#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <json-c/json.h>
#include "cJSON.h"
#include "check_event.h"

#define HANDLECOUNT  3
#define DOWN_HANDLE  0
#define PING_HANDLE  1
#define EVENT_HANDLE 2

/* File Name */
#define REFRESH_TOKEN_FILE_NAME				"/root/alexa/refresh_token"

#define I_AM_ALEXA_SOUND_FILE_NAME			"/root/alexa/i_am_alexa.wav"
#define WELCOME_FILE_NAME					"/root/alexa/welcome.wav"
#define UPLOAD_FILE_NAME					"/root/alexa/upload.wav"
#define START_SOUND_FILE_NAME				"/root/alexa/start_recording.wav"
#define FINISH_SOUND_FILE_NAME				"/root/alexa/finish_recording.wav"
#define NETWORK_ISSUE_SOUND_FILE_NAME		"/root/alexa/network_problem.wav"

#define HEAD_FILE_NAME						"/root/alexa/HeadFile"
#define BODY_FILE_NAME						"/root/alexa/BodyFile"
#define RESPONSE_FILE_NAME					"/root/alexa/response.wav"

#define DEL_HTTPHEAD_EXPECT					"Expect:"
#define DEL_HTTPHEAD_ACCEPT					"Accept:"

#define TOKEN_URL							"https://api.amazon.com/auth/O2/token"


/* User Account Information */
#define AUTH_CODE					"ANjzYZlHeuZYGrEBoSMC"
#define CODE_VERIFIER				"UVVzUv7JuHxkjvLeoFE8QCu2G3LkLxJv89dtQUCnz3qfwucx65HqShIiR3mGAVoA7j0go7sOO3X014A6wfmA9lxWzCzAk1MiSPRldkKNfPJzt8RimRkMBOW9kSKVP39B"
#define REDIRECT_URI				"amzn://com.compal.iot.minispeakerclient"
#define CLIENT_ID					"amzn1.application-oa2-client.acbf451e14ad461da4f2b1374bd343ef"

/* Status Code*/
#define STATUS_OK				200
#define STATUS_NO_CONTENT		204
#define STATUS_BAD_REQUEST		400
#define STATUS_FORBID			403


struct MemoryStruct {
	char *memory;
	size_t size;
};

typedef enum NET_STATE_T{
	NET_STATE_IDLE,
	NET_STATE_PING,
	NET_STATE_SEND_EVENT,
	NET_STATE_SEND_STATE,
	NET_STATE_WAIT_EVENT,
} net_state_t;


static struct MemoryStruct chunk;
static unsigned int data = 100;
static FILE *saveHeadFile = NULL;
static FILE *saveBodyFile = NULL;
static FILE *uploadFile = NULL;
static net_state_t netState;
static unsigned char count = 0;


static int write_token_to_file(char *refresh_token_in) {
	FILE *token_file = NULL;
	token_file = fopen(REFRESH_TOKEN_FILE_NAME, "w");
	if (token_file == NULL) {
		printf("[ERROR] Cannot open file %s for writing\n", REFRESH_TOKEN_FILE_NAME);
		return -1;
	}

	fwrite(refresh_token_in, 1, strlen(refresh_token_in), token_file);
	fclose(token_file);

	return 0;
}


static int read_token_from_file(char *refresh_token_out, int buf_size) {
	FILE *token_file = NULL;
	token_file = fopen(REFRESH_TOKEN_FILE_NAME, "r");
	char *no_use;

	if (token_file == NULL) {
		printf("[ERROR] Cannot open file %s for reading\n", REFRESH_TOKEN_FILE_NAME);
		return -1;
	}

	no_use = fgets(refresh_token_out, buf_size, token_file);
	fclose(token_file);

	return 0;
}


/* callback for curl fetch */
static size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;                           /* calculate buffer size */
	struct MemoryStruct *p = (struct MemoryStruct *) userp;   /* cast pointer to fetch struct */

	/* expand buffer */
	p->memory = (char *) realloc(p->memory, p->size + realsize + 1);

	/* check buffer */
	if (p->memory == NULL) {
		/* this isn't good */
		fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
		/* free buffer */
		free(p->memory);
		/* return */
		return -1;
	}

	/* copy contents to buffer */
	memcpy(&(p->memory[p->size]), contents, realsize);

	/* set new buffer size */
	p->size += realsize;

	/* ensure null termination */
	p->memory[p->size] = 0;

	/* return size */
	return realsize;
}


static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{  
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
	
	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}
	
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	printf("%s : size %zu \n %s \n", __FUNCTION__, realsize, (char *)contents);
	return realsize;
}


size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)   
{
	printf("write data begin...\n");
	int written = fwrite(ptr, size, nmemb, (FILE *)stream);
	fflush((FILE *)stream);

	return written;
}


size_t readFileFunc(char *buffer, size_t size, size_t nitems, void *instream)
{
	printf("begin freadfunc ... \n");
	int readSize = fread( buffer, size, nitems, (FILE *)instream );

	return readSize;
}


static void curl_ping_cfg(CURL *curl, struct curl_slist *head, char *auth)
{
	CURLcode res;

	/* set the opt */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0 );
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "/tmp/cacert.pem");

	/* set the head */
	head = curl_slist_append(head , DEL_HTTPHEAD_ACCEPT);
	head = curl_slist_append(head , "Path: /ping");
	head = curl_slist_append(head , auth);
	head = curl_slist_append(head , "Host: avs-alexa-na.amazon.com");
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);
	if (res != CURLE_OK) {
		printf("%s: curl_easy_setopt failed: %s\n", __FUNCTION__, curl_easy_strerror(res));
	}

	/* set the url */
	//curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-na.amazon.com/ping");
	curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-fe.amazon.com/ping");

	/* set the GET */
	//curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
}


static void curl_sync_state(CURL *curl, char *strJSONout,
								  struct curl_httppost *postFirst,
								  struct curl_httppost *postLast)
{
	char mid[50] = {0};
	cJSON *root, *context, *event, *header, *payload, *nonname, *tmpJson;

	uploadFile   = fopen( UPLOAD_FILE_NAME, "rb" );
	saveHeadFile = fopen( HEAD_FILE_NAME, "w+" );
	saveBodyFile = fopen( BODY_FILE_NAME, "w+" );
	/* set the json */
	root = cJSON_CreateObject();

	/* context */
	cJSON_AddItemToObject(root, "context", context = cJSON_CreateArray());
	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "AudioPlayer");
	cJSON_AddStringToObject(header, "name", "PlaybackState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddStringToObject(payload, "token", "");
	cJSON_AddNumberToObject(payload, "offsetInMilliseconds", 0);
	cJSON_AddStringToObject(payload, "playerActivity", "IDLE");

	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "Alerts");
	cJSON_AddStringToObject(header, "name", "AlertsState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddItemToObject(payload, "allAlerts", tmpJson = cJSON_CreateArray());
	cJSON_AddItemToObject(payload, "activeAlerts", tmpJson = cJSON_CreateArray());

	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "Speaker");
	cJSON_AddStringToObject(header, "name", "VolumeState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddNumberToObject(payload, "volume", 25);
	cJSON_AddFalseToObject(payload , "muted");

	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "SpeechSynthesizer");
	cJSON_AddStringToObject(header, "name", "SpeechState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddStringToObject(payload, "token", "");
	cJSON_AddNumberToObject(payload, "offsetInMilliseconds", 0);
	cJSON_AddStringToObject(payload, "playerActivity", "FINISHED");

	cJSON_AddItemToObject(root, "event", event = cJSON_CreateObject());
	cJSON_AddItemToObject(event, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "System");
	cJSON_AddStringToObject(header, "name", "SynchronizeState");
	snprintf(mid, 50, "messageId-%d", data);
	data++;
	cJSON_AddStringToObject(header, "messageId", mid);
	cJSON_AddItemToObject(event, "payload", payload = cJSON_CreateObject());

	strJSONout = cJSON_Print(root); 
	cJSON_Delete(root);

	printf("%s\n%zu\n", strJSONout, strlen(strJSONout));

	/*        curl set the formadd      */
	/*         JSON          */
	curl_formadd(&postFirst, &postLast,
				CURLFORM_COPYNAME, "metadata", /* CURLFORM_PTRCONTENTS, pAlexaJSON,  */
				CURLFORM_COPYCONTENTS, strJSONout,
				CURLFORM_CONTENTTYPE, "application/json; charset=UTF-8",
				CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFileFunc);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_data);  
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, saveHeadFile);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);  
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, saveBodyFile);
	
	/* set http post */
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, postFirst);
}


static void curl_send_audio_content(CURL *curl, char *strJSONout,
								struct curl_httppost *postFirst,
								struct curl_httppost *postLast)
{
	cJSON *root, *context, *event, *header, *payload, *nonname, *tmpJson;
	char mid[50] = {0};
	char did[50] = {0};
	int ret;

	printf("################################\n");
	printf("## START RECORDING FOR 3 SECS ##\n");
	printf("################################\n");
	ret = system("vspdump -m a -d 3");
	ret = system("sync");
	printf("################################\n");
	printf("##      FINISHED RECORDING    ##\n");
	printf("################################\n");

	/* Convert multi-channel to mono-channel */
	ret = system("ffmpeg -y -i output.wav -af \"pan=mono|c0=c2\" " UPLOAD_FILE_NAME);
	ret = system("sync");

	//ret = system("paplay --volume=50000 " FINISH_SOUND_FILE_NAME);

	uploadFile   = fopen( UPLOAD_FILE_NAME, "rb" );
	saveHeadFile = fopen( HEAD_FILE_NAME, "w+" );
	saveBodyFile = fopen( BODY_FILE_NAME,  "w+" );
	
	fseek( uploadFile, 0, SEEK_END);
	long uploadFileSize = ftell( uploadFile );
	fseek( uploadFile, 0, SEEK_SET);
	printf("uploadFileSize = %ld\n", uploadFileSize);
	/* set the json */
	root = cJSON_CreateObject();

	//=======context===========//
	cJSON_AddItemToObject(root, "context", context = cJSON_CreateArray() );
	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "AudioPlayer");
	cJSON_AddStringToObject(header, "name", "PlaybackState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddStringToObject(payload, "token", "");
	cJSON_AddNumberToObject(payload, "offsetInMilliseconds", 0);
	cJSON_AddStringToObject(payload, "playerActivity", "IDLE");
	
	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "Alerts");
	cJSON_AddStringToObject(header, "name", "AlertsState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddItemToObject(payload, "allAlerts", tmpJson = cJSON_CreateArray());
	cJSON_AddItemToObject(payload, "activeAlerts", tmpJson = cJSON_CreateArray());
	
	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "Speaker");
	cJSON_AddStringToObject(header, "name", "VolumeState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddNumberToObject(payload, "volume", 25);
	cJSON_AddFalseToObject(payload , "muted");
	
	cJSON_AddItemToArray(context , nonname = cJSON_CreateObject());
	cJSON_AddItemToObject(nonname, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "SpeechSynthesizer");
	cJSON_AddStringToObject(header, "name", "SpeechState");
	cJSON_AddItemToObject(nonname, "payload", payload = cJSON_CreateObject());
	cJSON_AddStringToObject(payload, "token", "");
	cJSON_AddNumberToObject(payload, "offsetInMilliseconds", 0);
	cJSON_AddStringToObject(payload, "playerActivity", "FINISHED");

	//========Events===========//
	cJSON_AddItemToObject(root, "event", event = cJSON_CreateObject());
	cJSON_AddItemToObject(event, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "SpeechRecognizer");
	cJSON_AddStringToObject(header, "name", "Recognize");
	snprintf(mid, 50, "messageId-%d", data);
	cJSON_AddStringToObject(header, "messageId", mid );//"messageId-123"
	snprintf(did, 50, "dialogRequestId-%d", data);
	data++;
	cJSON_AddStringToObject(header, "dialogRequestId", did );//"dialogRequestId-123"
	cJSON_AddItemToObject(event, "payload", payload = cJSON_CreateObject());
	cJSON_AddStringToObject(payload, "profile", "CLOSE_TALK");
	cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");

	//===========Tail===============//
	strJSONout = cJSON_Print(root); 
	cJSON_Delete(root);
	
	printf("%s\n%zu\n", strJSONout, strlen(strJSONout));

	/* formadd */

	//============josn================//
	curl_formadd(&postFirst, &postLast,
				CURLFORM_COPYNAME, "metadata", /* CURLFORM_PTRCONTENTS, pAlexaJSON,  */
				CURLFORM_COPYCONTENTS, strJSONout,
				CURLFORM_CONTENTTYPE, "application/json; charset=UTF-8",
				CURLFORM_END);

	//=============Audio=================//
	curl_formadd(&postFirst, &postLast,
				CURLFORM_COPYNAME, "audio",
				CURLFORM_STREAM, uploadFile,
				CURLFORM_CONTENTSLENGTH, uploadFileSize,
				CURLFORM_CONTENTTYPE, "application/octet-stream", //"audio/L16; rate=16000; channels=1",
				CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFileFunc);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_data);  
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, saveHeadFile);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);  
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, saveBodyFile);

	/* set http post */
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, postFirst);
}


static void curl_send_audio_cfg(CURL *curl, struct curl_slist *head, char *auth)
{
	/* set the opt */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0 );
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	//curl_easy_setopt(curl , CURLOPT_CAINFO, "/tmp/cacert.pem");

	/* set the head */
	head = curl_slist_append(head , DEL_HTTPHEAD_ACCEPT);
	head = curl_slist_append(head , DEL_HTTPHEAD_EXPECT);
	head = curl_slist_append(head , "Path: /v20160207/events");
	head = curl_slist_append(head , auth);
	head = curl_slist_append(head , "Content-type: multipart/form-data" );
	head = curl_slist_append(head , "Transfer-Encoding: chunked");
	head = curl_slist_append(head , "Host: avs-alexa-na.amazon.com");

	/* set the url */
	curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-fe.amazon.com/v20160207/events");
	//curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-na.amazon.com/v20160207/events");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);
}

static void curl_downchannel_cfg(CURL *curl, struct curl_slist *head, char *auth)
{
	CURLcode res;

	/* set the opt */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "/tmp/cacert.pem");

	/* set the head */
	head = curl_slist_append(head , DEL_HTTPHEAD_ACCEPT);
	head = curl_slist_append(head , "Path: /v20160207/directives");
	head = curl_slist_append(head , auth);
	head = curl_slist_append(head , "Host: avs-alexa-na.amazon.com");
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);
	if (res != CURLE_OK) {
		printf("%s: curl_easy_setopt failed: %s\n", __FUNCTION__,curl_easy_strerror(res));
	}

	/* set the url */
	curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-fe.amazon.com/v20160207/directives");
	//curl_easy_setopt(curl, CURLOPT_URL, "https://avs-alexa-na.amazon.com/v20160207/directives");

	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
}


static void curl_get_token(char *code,
			char *redirect_uri,
			char *client_id,
			char *code_verifier,
			char *refresh_token_out,
			char *access_token_out)
{
	CURL *ch;
	CURLcode rcode;
	json_object *json;
	enum json_tokener_error jerr = json_tokener_success;    /* json parse error */
	struct curl_slist *headers = NULL;
	struct MemoryStruct curl_fetch;
	struct MemoryStruct *cf = &curl_fetch;
	
	char *body_pattern = "grant_type=%s&code=%s&redirect_uri=%s&client_id=%s&code_verifier=%s";
	char body[1024];

	snprintf(body, sizeof(body), body_pattern,
		"authorization_code",
		code,
		redirect_uri,
		client_id,
		code_verifier
	);


	/* init curl handle */
	if ((ch = curl_easy_init()) == NULL) {
		/* log error */
		fprintf(stderr, 
			"ERROR: Failed to create curl handle in fetch_session\n");
	}


	/* Header */
	headers = curl_slist_append(headers, "POST /auth/o2/token HTTP/2");
	headers = curl_slist_append(headers, "Host: api.amazon.com");
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	headers = curl_slist_append(headers, "Cache-Control: no-cache");

	/* set curl options */
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0 );
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(ch, CURLOPT_URL, TOKEN_URL);

	/* set callback function */
	cf->memory = (char *) calloc(1, sizeof(cf->memory));
	cf->size = 0;
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) cf);

	/* fetch the url */
	rcode = curl_easy_perform(ch);

	/* cleanup curl handle */
	curl_easy_cleanup(ch);

	/* free headers */
	curl_slist_free_all(headers);


	/* check return code */
	if (rcode != CURLE_OK || cf->size < 1) {
		/* log error */
		fprintf(stderr, 
			"ERROR: Failed to fetch url (%s) - curl said: %s rcode = %u\n",
			TOKEN_URL, curl_easy_strerror(rcode), rcode);
		return;
	}

	/* check payload */
	if (cf->memory != NULL) {
		/* print result */
		printf("CURL Returned: \n%s\n", cf->memory);
		/* parse return */
		json = json_tokener_parse_verbose(cf->memory, &jerr);
		/* free payload */
		free(cf->memory);
	} else {
		/* error */
		fprintf(stderr, "ERROR: Failed to populate payload\n");
		/* free payload */
		free(cf->memory);
		return;
	}

	/* check error */
	if (jerr != json_tokener_success) {
		/* error */
		fprintf(stderr, "ERROR: Failed to parse json string");
		/* free json object */
		json_object_put(json);
		/* return */
		return;
	}

	json_object_object_foreach(json, key, json_entry) {
		if (strncmp(key, "refresh_token", 13) == 0) {
			strncpy(refresh_token_out, json_object_get_string(json_entry), 1024);
		} else if (strncmp(key, "access_token", 12) == 0) {
			strncpy(access_token_out, json_object_get_string(json_entry), 1024);
		}
	}

	json_object_put(json);

	write_token_to_file(refresh_token_out);
}


static void curl_refresh_token(char *refresh_token_in,
			char *redirect_uri,
			char *client_id,
			char *refresh_token_out,
			char *access_token_out)
{
	CURL *ch;
	CURLcode rcode;
	json_object *json;
	enum json_tokener_error jerr = json_tokener_success;    /* json parse error */
	struct curl_slist *headers = NULL;
	struct MemoryStruct curl_fetch;
	struct MemoryStruct *cf = &curl_fetch;
	
	char *body_pattern = "grant_type=%s&refresh_token=%s&redirect_uri=%s&client_id=%s";
	char body[1024];

	snprintf(body, sizeof(body), body_pattern,
		"refresh_token",
		refresh_token_in,
		redirect_uri,
		client_id
	);


	/* init curl handle */
	if ((ch = curl_easy_init()) == NULL) {
		/* log error */
		fprintf(stderr, 
			"ERROR: Failed to create curl handle in fetch_session\n");
	}


	/* Header */
	headers = curl_slist_append(headers, "POST /auth/o2/token HTTP/2");
	headers = curl_slist_append(headers, "Host: api.amazon.com");
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	headers = curl_slist_append(headers, "Cache-Control: no-cache");

	/* set curl options */
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0 );
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(ch, CURLOPT_URL, TOKEN_URL);

	/* set callback function */
	cf->memory = (char *) calloc(1, sizeof(cf->memory));
	cf->size = 0;
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) cf);

	/* fetch the url */
	rcode = curl_easy_perform(ch);

	/* cleanup curl handle */
	curl_easy_cleanup(ch);

	/* free headers */
	curl_slist_free_all(headers);


	/* check return code */
	if (rcode != CURLE_OK || cf->size < 1) {
		/* log error */
		fprintf(stderr, 
			"ERROR: Failed to fetch url (%s) - curl said: %s rcode = %u\n",
			TOKEN_URL, curl_easy_strerror(rcode), rcode);
		return;
	}

	/* check payload */
	if (cf->memory != NULL) {
		/* print result */
		printf("CURL Returned: \n%s\n", cf->memory);
		/* parse return */
		json = json_tokener_parse_verbose(cf->memory, &jerr);
		/* free payload */
		free(cf->memory);
	} else {
		/* error */
		fprintf(stderr, "ERROR: Failed to populate payload\n");
		/* free payload */
		free(cf->memory);
		return;
	}

	/* check error */
	if (jerr != json_tokener_success) {
		/* error */
		fprintf(stderr, "ERROR: Failed to parse json string");
		/* free json object */
		json_object_put(json);
		/* return */
		return;
	}


	json_object_object_foreach(json, key, json_entry) {
		if (strncmp(key, "refresh_token", 13) == 0) {
			strncpy(refresh_token_out, json_object_get_string(json_entry), 1024);
		} else if (strncmp(key, "access_token", 12) == 0) {
			strncpy(access_token_out, json_object_get_string(json_entry), 1024);
		}
	}

	json_object_put(json);

	/* So far, no need to update the token */
	//write_token_to_file(refresh_token_out);
}


static void reset_curl_handles(const char *access_token,
								struct curl_slist *eventHead,
								struct curl_slist *downHead,
								struct curl_slist *pingHead,
								CURL **handles)
{
	char auth[1024] = "Authorization:Bearer ";
	strcat(auth, access_token);

	/* Free the custom headers */
	if (downHead) curl_slist_free_all(downHead);
	if (pingHead) curl_slist_free_all(pingHead);
	if (eventHead) curl_slist_free_all(eventHead);

	/* Apply new token to each handles */
	curl_send_audio_cfg(handles[EVENT_HANDLE], eventHead, auth);
	curl_downchannel_cfg(handles[DOWN_HANDLE], downHead, auth);
	curl_ping_cfg(handles[PING_HANDLE], pingHead, auth);
}


#define HTTP2 "HTTP/2"
#define DATA_SIZE  100

static int is_rcv_ok(void)
{
	int ret = 0;
	FILE *hfd = NULL;
	char str[DATA_SIZE] = {0};
	char *no_use;

	hfd = fopen(HEAD_FILE_NAME , "rb");
	no_use = fgets(str, DATA_SIZE, hfd);
	fclose(hfd);
	sscanf(str, "HTTP/2 %d ", &ret);

	return ret;
}


int main(int argc, char **argv)
{
	int ret = 0, first_time = 0;
	int total_cnt = 0, fail_400_cnt = 0, fail_403_cnt = 0, ok_cnt = 0;

	/* the curl variable */
	CURL *handles[HANDLECOUNT];
	CURLM *multi_handle;
	int i;
	int still_running;	/* keep number of running handles */
	CURLMsg *msg;		/* for picking up messages with the transfer status */
	int msgs_left;		/* how many messages are left */
	
	char refresh_token[1024] = "";
	char access_token[1024] = "";
	
	/* for the event variable */
	char *strJSONout = NULL;
	struct curl_httppost *postFirst = NULL, *postLast = NULL;
	struct curl_slist *downHead = NULL;
	struct curl_slist *pingHead = NULL;
	struct curl_slist *eventHead = NULL;

	/* Library Version */
	printf("curl_ver: %s\n", curl_version());

	/* Argument */
	printf("argv[0] = %s\n", argv[0]);
	printf("argv[1] = %s\n", argv[1]);
	printf("argv[2] = %s\n", argv[2]);
	printf("argv[3] = %s\n", argv[3]);

	
	if (argv[1]) {
		printf("Input Authorization Code: %s\n", argv[1]);
		curl_get_token(
			AUTH_CODE,
			//argv[1],
			REDIRECT_URI,
			CLIENT_ID,
			CODE_VERIFIER,
			refresh_token,
			access_token
		);
	}
	
	read_token_from_file(refresh_token, sizeof(refresh_token));

	if (strncmp(refresh_token, "", 1024) == 0) {
		printf("Please update the AUTHO_CODE and CODE_VERIFIER then run ./avs 1 in 5 minutes.");
		return 0;
	}

	/* Keep asking access token utill network is ready */
	do {
		sleep(1);
		printf("Try to get access token: %s\n", access_token);
		curl_refresh_token(
			refresh_token,
			REDIRECT_URI,
			CLIENT_ID,
			refresh_token,
			access_token
		);
	} while (strncmp(access_token, "", 1024) == 0);

	printf("################################\n");
	printf("## refresh_token = %s\n", refresh_token);
	printf("## access_token  = %s\n", access_token);
	printf("################################\n");


	/* initialize curl */
	/* init a multi stack */
	multi_handle = curl_multi_init();
	/* Allocate one CURL handle per transfer */
	for (i = 0; i < HANDLECOUNT; i++) {
		handles[i] = curl_easy_init();
	}


	chunk.memory = malloc(1);	/* will be grown as needed by the realloc above */
	chunk.size = 0;				/* no data at this point */


	reset_curl_handles(access_token, eventHead, downHead, pingHead, handles);


	/* Add each handles to the multi_handle */
	curl_multi_add_handle(multi_handle, handles[DOWN_HANDLE]);
	curl_multi_add_handle(multi_handle, handles[PING_HANDLE]);

	/********************run***************************************/
	curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
	/* We do HTTP/2 so let's stick to one connection per host */
	curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);
	/* we start some action by calling perform right away */
	curl_multi_perform(multi_handle, &still_running);


	/* Make a FIFO for wake-up event */
	ret = system("rm /root/alexa/wakeup");
	ret = system("sync");
	if (mkfifo("/root/alexa/wakeup", 0644) == -1) {
		printf("mkfifo error\n");
		return 1;
	}


	do 
	{
		struct timeval timeout;
		int rc;/* select() return code */
		int event;
		CURLMcode mc; /* curl_multi_fdset() return code */

		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;
		int maxfd = -1;

		long curl_timeo = -1;


		switch (netState)
		{
			case NET_STATE_WAIT_EVENT:

				printf("SUCCESS COUNT ==> %d/%d/%d/%d\n", ok_cnt, fail_400_cnt, fail_403_cnt, total_cnt);

				printf("#################################\n");
				printf("#### Press Button to SPEAK!! ####\n");
				printf("#################################\n");

				/* Play some ring tone for the first time when alexa is ready. */
				if (first_time == 0) {
					//ret = system("paplay --volume=50000 " WELCOME_FILE_NAME);
					first_time = 1;
				}

				/* Will be blocked here, waiting for button pressed or timeout */
				ret = system("/root/fb_test /root/wakeup.rgb > /dev/null");
				event = check_event();
				switch (event) {
					case EVENT_TIMEOUT:
						netState = NET_STATE_PING;
						break;
					case EVENT_ROKID:
					case EVENT_BUTTON:
						ret = system("/root/fb_test /root/listening_en.rgb > /dev/null");
						ret = system("paplay --volume=50000 " START_SOUND_FILE_NAME);
						netState = NET_STATE_SEND_EVENT;
						break;
				}
				break;
			case NET_STATE_IDLE:
				break;
			case NET_STATE_PING:
				curl_multi_add_handle(multi_handle, handles[PING_HANDLE]);
				netState = NET_STATE_IDLE;
				break;
			case NET_STATE_SEND_STATE:
			{
				printf("start send the state ~~~~~~\n");
				curl_sync_state(handles[EVENT_HANDLE], strJSONout, postFirst, postLast);
				curl_multi_add_handle(multi_handle, handles[EVENT_HANDLE]);
				netState = NET_STATE_IDLE;
			}
				break;
			case NET_STATE_SEND_EVENT:
			{
				curl_send_audio_content(handles[EVENT_HANDLE], strJSONout, postFirst, postLast);
				curl_multi_add_handle(multi_handle, handles[EVENT_HANDLE]);	
				netState = NET_STATE_IDLE;
			}
				break;
			default:
				break;
		}


		/* set the multi_curl_handle */
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		/* set a suitable timeout to play around with */
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		curl_multi_timeout(multi_handle, &curl_timeo);
		if (curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if (timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		/* get file descriptors from the transfers */
		mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

		if (mc != CURLM_OK) {
			fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
			break;
		}
		
		/* On success the value of maxfd is guaranteed to be >= -1. We call
		   select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
		   no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
		   to sleep 100ms, which is the minimum suggested value in the
		   curl_multi_fdset() doc. */
		if (maxfd == -1) {
			/* Portable sleep for platforms other than Windows. */
			struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
			rc = select(0, NULL, NULL, NULL, &wait);
		}
		else {
			/* Note that on some platforms 'timeout' may be modified by select().
			If you need access to the original value save a copy beforehand. */
			rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		switch (rc) {
			case -1:
				/* select error */
				printf("~~~~~this is error~~~\n");
				break;
			case 0:
			default:
				/* timeout or readable/writable sockets */
				curl_multi_perform(multi_handle, &still_running);
				break;
		}


		/* See how the transfers went */
		while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				int idx, found = 0;

				/* Find out which handle this message is about */
				for (idx=0; idx<HANDLECOUNT; idx++) {
					found = (msg->easy_handle == handles[idx]);
					if (found) {
						break;
					}
				}

				switch (idx) {

					case DOWN_HANDLE:
						printf("DOWN_HANDLE completed with status %d\n", msg->data.result);
						curl_multi_add_handle(multi_handle, handles[DOWN_HANDLE]);
						break;

					case PING_HANDLE:
						printf("PING_HANDLE completed with status %d\n", msg->data.result);
						/* after send the ping, then remove the handle */
						curl_multi_remove_handle(multi_handle , handles[PING_HANDLE]);

						printf("Refresh Token: %s\n", refresh_token);
						curl_refresh_token(refresh_token,
											REDIRECT_URI,
											CLIENT_ID,
											refresh_token,
											access_token);
						reset_curl_handles(access_token, eventHead, downHead, pingHead, handles);

						/* just change this  */
						netState = NET_STATE_WAIT_EVENT;  // Waiting for next event
						break;

					case EVENT_HANDLE:
						printf("EVENT_HANDLE completed with status %d\n", msg->data.result);
						curl_multi_remove_handle(multi_handle , handles[EVENT_HANDLE]);
						fclose(uploadFile);
						fclose(saveHeadFile);
						fclose(saveBodyFile);
						if (!(ret = is_rcv_ok())) {
							printf("delete the mp3Ring...\n");
						}
						free(strJSONout);
						strJSONout = NULL;
						curl_formfree( postFirst);
						printf("-----finished count %d\n", count++);

						total_cnt++;

						printf("Got Result : %d\n", ret);
						if (ret == STATUS_OK || ret == STATUS_NO_CONTENT) {

							ret = system("/root/fb_test /root/answering_en.rgb > /dev/null");
							ret = system("sync");
							ret = system("ffmpeg -y -i " BODY_FILE_NAME " " RESPONSE_FILE_NAME);
							ret = system("sync");
							ret = system("paplay --volume=50000 " RESPONSE_FILE_NAME);
							ret = system("mv " BODY_FILE_NAME " " BODY_FILE_NAME".bak");
							ret = system("mv " RESPONSE_FILE_NAME " " RESPONSE_FILE_NAME".bak");
							ok_cnt++;

						} else if (ret == STATUS_BAD_REQUEST) {

							printf("[ERROR] Network is unreachable!!\n");
							ret = system("paplay --volume=50000 " NETWORK_ISSUE_SOUND_FILE_NAME);
							fail_400_cnt++;

						} else if (ret == STATUS_FORBID) {

							/* Token Expired, refresh token!! */
							printf("Refresh Token: %s\n", refresh_token);
							curl_refresh_token(refresh_token,
												REDIRECT_URI,
												CLIENT_ID,
												refresh_token,
												access_token);

							reset_curl_handles(access_token, eventHead, downHead, pingHead, handles);
							fail_403_cnt++;
						}
						netState = NET_STATE_WAIT_EVENT;  // Waiting for next event

						break;

					default:
						break;
				}
			}
		}

	} while(1);

	/* clean up */
	curl_multi_cleanup(multi_handle);
	/* Free the CURL handles */
	for (i=0; i<HANDLECOUNT; i++)
		curl_easy_cleanup(handles[i]);

	/* free the custom headers */
	curl_slist_free_all(downHead);
	curl_slist_free_all(pingHead);
	curl_slist_free_all(eventHead);

	return ret;
}
