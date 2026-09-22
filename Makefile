SERVER = server
CLIENT = client

$(SERVER): server.cpp
	g++ server.cpp -o $(SERVER) -pthread

$(CLIENT): client.cpp
	g++ client.cpp -o $(CLIENT) -pthread

make: $(SERVER) $(CLIENT)

run: $(SERVER) $(CLIENT)
	./$(CLIENT) 127.0.0.1 8080

clean:
	rm -f $(SERVER) $(CLIENT)