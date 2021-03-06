/*
 * webserver.cpp
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#include "webserver.h"

using namespace std;

Webserver::Webserver(string file="", int port=8080){
	fileName = file;
	//Assign reasonable defaults to config parameters
	servicePort = port;
	documentRoot = ".";
	documentIndex.insert("index.html");
	documentIndex.insert("index.htm");
	contentTypes[".html"] = "text/html";
	if(file != ""){
		parseFile(file);
	}
	cout << "Current configuration options:"<<endl;
	cout << "Service port: " << servicePort << endl;
	cout << "Document root: " << documentRoot << endl;
	for(set<string>::iterator i = documentIndex.begin(); i != documentIndex.end(); i++){
		cout << "Document index: " << *i <<endl;
	}
	for(map<string, string>::iterator i = contentTypes.begin(); i != contentTypes.end(); i++){
		cout << "Content type: " << i->first << " => " << i->second << endl;
	}
}

Webserver::~Webserver(){
}

int Webserver::parseFile(string file){
	cout << "Parsing configuration file: "<<file<<endl;
	ifstream configFile(file.c_str());
	if(!configFile.is_open()){
		cout << "Error opening config file" << endl;
		return -1;
	}

	set<ConfigOptions> parsedOptions;
	string line;
	ConfigOptions current;
	bool parseSuccess = false;
	int lineCounter = 0;
	while(!configFile.eof()){
		if(getline(configFile, line)){
			//check if it is the "Listen" parameter
			if(line.find("Listen") == 0){
				current = ServicePort;
			}
			//check if it is the "DocumentRoot" parameter
			else if(line.find("DocumentRoot") == 0){
				current = DocumentRoot;
			}
			//check if is the "DirectoryIndex" parameter
			else if(line.find("DirectoryIndex") == 0){
				current = DirectoryIndex;
			}
			//check if it is a "Content-Type" option
			else if(line.find(".") == 0){
				current = ContentType;
			}
			else{
				lineCounter++;
				continue;
			}
			parseSuccess = parseOption(line, current);
			if(!parseSuccess){
				cout << "Could not parse option: " + current << " at line: " << lineCounter << endl;
			}
		}
		lineCounter++;
	}
	return 0;
}

//parse a string assuming it is the option specified by option
bool Webserver::parseOption(string line, ConfigOptions option){
	switch(option){
		case ServicePort:
		{
			int port;
			if(sscanf(line.c_str(), "Listen %i", &port) == EOF){
				cout << "Could not parse service port" << endl;
				return false;
			}
			servicePort = port;
			return true;
		}
		case DocumentRoot:
		{
			char docBuffer[1024];
			if(sscanf(line.c_str(), "DocumentRoot %s", docBuffer) == EOF){
				cout << "could not parse Document Root" << endl;
				return false;
			}
			documentRoot = string(docBuffer);
			int firstOccurence = documentRoot.find("\"");
			documentRoot = documentRoot.replace(firstOccurence, firstOccurence + 1, "");
			int secondOccurrence = documentRoot.find("\"");
			documentRoot = documentRoot.replace(secondOccurrence, secondOccurrence + 1, "");
			return true;
		}
		case DirectoryIndex:
		{
			char* tok = strtok((char*)line.c_str(), " ");
			if(strcmp(tok, "DirectoryIndex") != 0){
				cout << "Could not parse Directory Index" << endl;
				return false;
			}
			while(tok != NULL){
				if(strcmp(tok, "DirectoryIndex") != 0){
					documentIndex.insert(tok);
				}
				tok = strtok(NULL, " ");
			}
			return true;
		}
		case ContentType:
		{
			if(line.find(".") != 0){
				cout << "Could not parse content type" << endl;
				return false;
			}
			char fileType[1024];
			char contentType[1024];
			if(sscanf(line.c_str(), "%s %s", fileType, contentType) == EOF){
				cout << "Could not parse content type" << endl;
				return false;
			}
			contentTypes[fileType] = contentType;
			return true;
		}
	}
	return false;
}

/**
 * this function will handle a GET request contained in the request variable that is associated
 * with the input socket descriptor. it uses passed in variables for document root, indices and
 * content types. this is likely not going to work on non-linux system due to both the file
 * system calls and how the file path separators are uses. if cross platform support is needed,
 * a higher-level language with platform-independent file system functionality should be used
 * instead of c++
 *
 * this is the only method handling function, as per the email
 * from Sangtae on Sept. 15, 2015:

	Hi all,

	The deadline for the programming assignment #1 is extended to 9/17
	(one week extension). Even you do not know about socket programming,
	you can learn and finish the assignment on time (just start from
	BeeJ's guide). Also don't be shy to post any questions in the
	discussion board. This assignment is the important precursor for the
	other three assignments down the road, so please spend time and
	complete it by yourself.

	Also a few clarifications:

	1) You do not need to implement POST and other requests. You only have
	to implement GET.

	2) after the step (4) in the previous email below, you have to check
	whether your web browser (chrome/firefox/safari) displays the content
	your echo/web server sends. After that, you handle

	3) You may not handle Error Code 501, such as "HTTP/1.1 501 Not
	Implemented: /csc573/video/lecture1.mpg."

	Best,
	Sangtae

 */
static int handleGet(int socket_fd, string request, string documentRoot, set<string> documentIndex, map<string, string> contentTypes, bool keep_alive){
	int exists, accessible;
	string currentContentType;
	bool found = false;

	//tokenize the request to get the method, uri and http version
	char* method = strtok((char*)((request).c_str()), " ");
	char* uri = strtok(NULL, " ");
	char* httpVersion = strtok(NULL, " ");

	/**
	 * make sure that these values are valid and are what is
	 * required and what we support for this assignment
	 * this should be caught before now, but check again to
	 * be safe
	 */
	if(method == NULL){
		write400BadMethod(socket_fd, "");
		return -1;
	} else if(strncmp(method, "GET", 3) != 0){
		write400BadMethod(socket_fd, string(method));
	}
	if(uri == NULL){
		//write bad uri
		write400BadUri(socket_fd, "");
		return -1;
	}
	//check that http version is not null and that it
	//is one that we support. since HTTP 1.0 and 1.1
	//are similar enough for the scope of this assignment
	//support both
	if(httpVersion == NULL){
		//write bad http
		write400BadHttpVersion(socket_fd, "");
		return -1;
	} else if(strncmp(httpVersion, "HTTP/1.1", 8) != 0 && strncmp(httpVersion, "HTTP/1.0", 8) != 0){
		cout << "TEST-----------------"<<endl;
		cout << httpVersion <<endl;
		write400BadHttpVersion(socket_fd, string(httpVersion));
		return -1;
	}

	//start building up file path from document root
	string filePath = documentRoot;
	filePath = filePath.append("/");

	//add the uri to the file path
	filePath = filePath.append(uri);

	//initial check to see if the uri exists and is accessible
	checkFilepathExistsAccessible(filePath, &exists, &accessible);

	string contentType = "";
	//if the uri is not a directory, is accessible and exists,
	//then check if it is for a valid content type
	if(!isDirectory(filePath) && (exists == 1 && accessible == 1)){
		//need to determine the content type and then vomit over the socket
		found = true;
		//need to check the content types. if the uri has a file extension, then
		//can iterate through the content types. if not, then is an index and should continue.
		//if has a file type and does not match an entry in the content-types,
		//send 501: not implemented
		for(map<string, string>::iterator i = contentTypes.begin(); i != contentTypes.end(); i++){
			if(endsWith((char*)(filePath.c_str()), (char*)((i->first).c_str()))){
				contentType = i->second;
			}
		}

		if(contentType == ""){
			write501(socket_fd, string(uri));
		} else{
			vomitFileToSocketHttp(socket_fd, filePath, contentType, keep_alive);
		}
		return 1;
	}
	//if we did not go into the last if, then we could still be looking for
	//a directory index. if it was a directory, or it did not exist and was
	//not accessible, try document indices
	if(isDirectory(filePath) || (exists == 0 && accessible == 0)){
		//need to try the documentIndex's
		filePath = filePath.append("/");
		for(set<string>::iterator i = documentIndex.begin(); i != documentIndex.end(); i++){
			string currentIndex = string(filePath).append(*i);
			//check if this document index exists
			checkFilepathExistsAccessible(currentIndex, &exists, &accessible);
			if(exists == 1 && accessible == 1){
				found = true;
				//vomit the index over the socket with content type = html
				vomitFileToSocketHttp(socket_fd, currentIndex, "text/html", keep_alive);
				return 1;
			}
		}

	}

	//if we never found the file, then we need to send an error code
	if(!found){
		//TODO: may need to store the output of the access call
		//if the file does not exist, then 404
		//if it did exist, but was unaccessible, 403
		//other possibilites should not occur
		if(exists == 0){
			//send 404
			write404(socket_fd, string(uri));
			 return -1;
		} else if(exists == 1 && accessible == 0){
			//send 403
			write403(socket_fd);
			return -1;
		} else{
			cout << "Am in impossible state where either file"
					" was found and was not sent without encountering a return"
					"or the file is accessible but does not exist." <<endl;
			return -1;
		}
	}
	return 1;

}

/**
 * The function that is executed by a worker thread
 */
static void* workerThreadTask(void* workerArgs){
	struct workerArgs *args = (struct workerArgs*)(workerArgs);
	int requestType;

	//set up the variables for the sleep function
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;

	//lock and then read the control variables
	pthread_mutex_lock(args->lock);
	volatile bool localContinue = args->continue_processing;
	volatile bool localFlush = args->flush_queue;
	pthread_mutex_unlock(args->lock);

	//loop on the local control variables, as is difficult
	//to lock in while loop condition
	while(localFlush || localContinue){
		//flag to skip writing if there is nothing in the queue
		bool skip = false;
		struct QueueItem* requestItem;

		//check if queue is empty and whether we should continue
		if(args->workQueue->getSize() <= 0){
			if(localContinue){
				skip = true;
			} else{
				break;
			}
		} else{
			//get the first element off the queue
			//skip if for some reason this is null
			requestItem = args->workQueue->pop();
			if(requestItem == NULL){
				skip = true;
			}
		}

		//write the response if there was a request
		if(!skip){
			string request = requestItem->request;
			char tokRequest[request.size()];
			//need to make a copy, as previous implementations had weird errors here
			strcpy(tokRequest, request.c_str());

			//final check of request headers
			char* request_type_str = strtok(tokRequest, " ");
			char* uri = strtok(NULL, " ");
			char* http_version = strtok(NULL, " ");
			if(request_type_str == NULL){
				//invalid method
				write400BadMethod(args->socket_fd, "");
				continue;
			}
			if(uri == NULL){
				cout << "Bad uri. Line: "<<request<<endl;
				write400BadUri(args->socket_fd, "");
				continue;
			}
			if(http_version == NULL){
				write400BadHttpVersion(args->socket_fd, "");
				continue;
			}

			//check that this is the correct method
			if(strncmp(request_type_str, "GET", 3) == 0){
				requestType = 0;
			}

			//if is GET, the process it. else, sent 400 bad method
			if(requestType == 0){
				bool keep_alive= false;
				if(request.find("keep-alive") != request.npos){
					keep_alive = true;
				}
				int keepConnection = handleGet(args->socket_fd, request, args->documentRoot, args->documentIndex, args->contentTypes, keep_alive);
				if(keepConnection < 0){
					break;
				}
			} else{
				cout << "Not implemented yet" << endl;
				write400BadMethod(args->socket_fd, request_type_str);
			}
			free(requestItem);
		}

		//update local control variables
		pthread_mutex_lock(args->lock);
		localContinue = args->continue_processing;
		localFlush = args->flush_queue;
		pthread_mutex_unlock(args->lock);
		nanosleep(&sleepTime, &sleepTimeRem);
	}
	pthread_mutex_lock(args->worker_end_lock);
	args->workerEndConnection = true;
	pthread_mutex_unlock(args->worker_end_lock);
	pthread_exit(NULL);
}

/**
 *	Function to be executed by a pthread to handle a connection. Connection
 *	is passed as a file descriptor in the struct. This function will spawn
 *	a separate worker thread. Will then read from the socket at intervals
 *	and put data into a queue to be processed. This allows for the pipelining
 *	to be performed easily
 */
static void *handleConnection(void* connectionArgs){
	char recv_buffer[MAX_REQUEST_SIZE+1];
	struct connectionArgs *args = (struct connectionArgs*)(connectionArgs);
	int bytesReceived;
	time_t start;
	bool waitForTimer = false, continueReceving = true, isFirst = true;

	ConcurrentQueue *workQueue = new ConcurrentQueue;
	pthread_mutex_t* lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_t* worker_end_lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));

	pthread_mutex_init(lock, NULL);
	pthread_mutex_init(worker_end_lock, NULL);

	/**
	 * setup worker thread pthread variables and make joinable
	 * need to be joinable so that we can ensure that all work
	 * has been done by the worker before we close the socket
	 * when finished
	 */
	pthread_t worker_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	//setup inputs to the worker thread
	struct workerArgs *workerArgsStr = new workerArgs;
	workerArgsStr->contentTypes = args->contentTypes;
	workerArgsStr->continue_processing = true;
	workerArgsStr->documentIndex = args->documentIndex;
	workerArgsStr->documentRoot = args->documentRoot;
	workerArgsStr->socket_fd = args->connection_fd;
	workerArgsStr->workQueue = workQueue;
	workerArgsStr->flush_queue = false;
	workerArgsStr->lock= lock;
	workerArgsStr->workerEndConnection = false;
	workerArgsStr->worker_end_lock = worker_end_lock;

	/**
	 * spawn the worker thread immediatly so that is ready for work
	 * when we read from the socket
	 */
	pthread_create(&worker_thread, &attr, workerThreadTask, (void*)workerArgsStr);

	/**
	 * initialize the timer variables
	 * need to use the time() function since the thread sleeps
	 * meaning that calls to clock() will not give info we need
	 */
	start = time(NULL);

	/**
	 * keep looping until the timer expires. the timer will expire after
	 * 10 seconds after the first successful read from the socket. if a
	 * keep-alive header is detected then the timer is reset
	 */
	bool keep_alive = false;
	string request;
	while(continueReceving){
		pthread_mutex_lock(worker_end_lock);
		if(workerArgsStr->workerEndConnection){
			break;
		}
		pthread_mutex_unlock(worker_end_lock);

		struct timespec sleepTime;
		sleepTime.tv_sec = 0;
		sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
		struct timespec sleepTimeRem;

		//if its not the first, and keep alive is not set, break out
		if(!isFirst && !keep_alive){
			break;
		}
		/**
		 * perform a non-blocking read on the buffer
		 * if we read a non-zero # of bytes, then begin timer and
		 * send the request to the worker
		 */
		//clear the receive buffer
		memset(recv_buffer, 0, MAX_REQUEST_SIZE);
		//TODO: detect when the end of the request occurs and build up request string until then

		bytesReceived = recv(args->connection_fd, recv_buffer, MAX_REQUEST_SIZE, MSG_DONTWAIT);
		if(bytesReceived > 0){
			char currentReceivedBuffer[bytesReceived];
			memcpy(currentReceivedBuffer, recv_buffer, bytesReceived);
			//make sure there is a null terminator to avoid overflows
			recv_buffer[bytesReceived] = '\0';

			//append the currently received bytes to the running request string
			request.append(currentReceivedBuffer, bytesReceived);

			//need to check if we received a blank line, as this indicates that the request headers have been read
			//since we do not support a message body in this webserver, can stop there
			char toFind[] = {0xd, 0xa, 0xd, 0xa, 0x0};
			if(request.find(toFind) == request.npos){
				continue;
			}

			/**
			 * check if a keep-alive header is present. if so, need to reset timer
			 * to now and signal to worker that this is a keep-alive request. also
			 * make sure that we know that we are waiting for a timer to expire now
			 *
			 * if is not present, still start the timer if this is the first request
			 * we've received in the on the socket, else make sure we are not waiting
			 * for a timer
			 */
			if(request.find("Connection: keep-alive") != request.npos){
				keep_alive = true;
				waitForTimer = true;
				isFirst = false;
				start = time(NULL);
			} else if(isFirst){
				waitForTimer = false;
				start = time(NULL);
				isFirst = false;
			}
			else{
				waitForTimer = false;
				continueReceving = false;
			}
			/**
			 * check that the method is GET. as per email from Sangtae, we only
			 * need to implement the GET method for this assignment. therefore,
			 * should only put requests that are GET methods onto the work queue
			 */
			//first make sure that the strtok did not fail

			string clone = string(request.c_str());
			char* method = strtok((char*)clone.c_str(), " ");

			if(method == NULL){
				//if so, send bad method
				write400BadMethod(args->connection_fd, "");
				break;
			} else if(strncmp(method, "GET", 3) == 0){
				/**
				 * put the request onto the work queue.
				 * should notify the worker thread that
				 * data is available automatically
				 *
				 * make a deep copy of the string so that
				 * we can reset the request
				 */
				workQueue->push(string(request.c_str()), keep_alive);
				request = "";
			} else{
				/**
				 * send am invalid method message. since strtok is supposed to return
				 * a valid c_str, this should not fail
				 */
				write400BadMethod(args->connection_fd, string(method));
				break;
			}
		}

		/**
		 * if we are waiting for a timer, need to check if the timer has expired
		 * if so, need to make sure we don't do any more iterations
		 */
		if(waitForTimer){
			time_t current = time(NULL);
			double difference = difftime(current, start);
			double seconds = difference;// / CLOCKS_PER_SEC;
			if(seconds >= 10){
				//timer expired
				continueReceving = false;
			}
		}
		//sleep for a time so we don't waste the cpu
		nanosleep(&sleepTime, &sleepTimeRem);

	}
	/**
	 * need to now cleanup. for one of many reasons, the previous logic has
	 * decided that we need to close the socket. first need to notify the worker
	 * thread to wake up and process any remaining data in the work thread. then
	 * need to join on the thread and wait for it to complete before closing socket
	 * this is why we need the lock, so that there is synchronization of the read
	 * of the boolean control variables of the worker
	 */

	//lock access to control
	pthread_mutex_lock(lock);
	//signal thread to stop processing after emptying the queue
	workerArgsStr->continue_processing = false;
	workerArgsStr->flush_queue = true;
	void* status;

	//tell the queue to notify any waiting threads to wake
	workQueue->toggleQueue();
	workQueue->wakeQueue();

	//release control lock
	pthread_mutex_unlock(lock);

	//join on worker thread
	pthread_join(worker_thread, &status);

	//finally, close socket when worker exits
	cout << "Closing socket: "<<args->connection_fd<<endl;
	close(args->connection_fd);

	//cleanup and exit
	delete workQueue;
	pthread_mutex_destroy(lock);
	free(lock);
	pthread_exit(NULL);
}

/**
 *	Runs the HTTP server on the TCP service port provided in the config file
 */
int Webserver::runServer(){
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;
	list<pthread_t*> threads;
	struct sockaddr_in server_addr;
	char service[100];
	sprintf(service, "%i", servicePort);

	//setup struct to create socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(servicePort);

	cout << "Running server" << endl;

	//request socket from system
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		return socket_fd;
	}

	//set socket reusable from previous program run
	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//bind to socket
	status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		cout << "Error binding on socket" << endl;
		return status;
	} else{
		cout << "Running HTTP server on TCP port: "<<servicePort<<endl;
	}

	//listen on socket with a backlog of 10 connections
	status = listen(socket_fd, 100);
	if(status < 0){
		cout << "Error listening on bound socket" << endl;
		return status;
	} else{
		cout << "Listening on TCP socket" << endl;
	}

	while(true){
		//accept connections until program terminated
		char clientIp[INET_ADDRSTRLEN];
		cout << "Accepting new connections" << endl;

		struct sockaddr connection_addr;
		socklen_t connection_addr_size = sizeof(sockaddr);

		//block on accept until new connection
		new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);
		if(new_connection_fd < 0){
			cout << "Error accepting connection" <<endl;
		}

		//calculate ip of client
		int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
		inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
		cout << "Accepted new connection from: " << string(clientIp) << " in socket: " << new_connection_fd << endl;

		//spin off new pthread to handle connection

		//setup thread and inputs
		pthread_t* current_connection_thread = (pthread_t*)malloc(sizeof(pthread_t));
		struct connectionArgs *args = new connectionArgs;
		args->thread_id = thread_ids;
		args->connection_fd = new_connection_fd;
		args->documentRoot = documentRoot;
		args->documentIndex = documentIndex;
		args->contentTypes= contentTypes;

		//spawn thread. we do not wait on these, since we do not care when they finish
		//these threads handle closing the socket themselves
		rc = pthread_create(current_connection_thread, NULL, handleConnection, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
		thread_ids++;
		threads.push_back(current_connection_thread);
	}
	return 0;
}

void printHelp(){
	cout << "Usage: webserver [-f FILE] [-p PORT] [-h]" << endl;
	cout << "Options:" << endl;
	cout << "  -f FILE\tSpecify a configuration file. If the file cannot be opened, reasonable defaults will be assumed. See README for format. The default is ./ws.conf."<<endl;
	cout << "  -p PORT\tSpecify a service port. Will be ignored if port is set in a configuration file. The default is 8080." <<endl;
	cout << "  -h\t\tPrint this message and exit."<<endl;
}

int main(int argc, char** argv){
	int opt;
	string configFile = "./ws.conf";
	int port = 8080;
	if(argc > 1){
		while( (opt = getopt(argc, argv, "f:p:h")) != -1){
			switch(opt){
				case 'f':
					configFile = string(optarg);
					break;
				case 'p':
					port = atoi(optarg);
					break;
				case 'h':
					printHelp();
					return -1;
				case '?':
					printHelp();
					return -1;
			}
		}
	}
	Webserver server(configFile, port);
	cout << "Return code: " << server.runServer() << endl;
	return 0;
}
