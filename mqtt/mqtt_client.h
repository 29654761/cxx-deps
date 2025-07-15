#pragma once

extern "C" {
#include <MQTTClient.h>
#include <MQTTClientPersistence.h>
#include <MQTTAsync.h>
}

#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <string>
#include <map>
#include <mutex>

class mqtt_client
{
	struct topic_t
	{
		std::string topic;
		int qos = 0;
		bool state = false;
	};


	struct sub_context_t
	{
		std::string topic;
		void* ctx = nullptr;
	};

public:
	typedef void (*connected_event_t)(void* context);
	typedef void (*disconnect_event_t)(void* context);
	typedef void (*reconnecting_event_t)(void* context, std::string& uri, std::string& client_id, std::string& user, std::string& password);
	typedef void (*message_event_t)(void* context, const std::string& topic, const std::string& payload, int qos);

	mqtt_client();
	~mqtt_client();

	bool start(const std::string& uri, const std::string& client_id, const std::string& user = "", const std::string& password = "");

	void stop();

	bool reconnect(const std::string& uri, const std::string& client_id, const std::string& user = "", const std::string& password = "");

	bool subscribe(const std::string& topic, int qos = 2);

	void unsubscribe(const std::string& topic);

	void clear_subscribes();

	bool publish(const std::string& topic, const std::string& message, int qos = 0, bool retained = false);

	bool is_connected()const;



private:

	bool _subscribe(topic_t& tp);

	bool set_topic_state(const std::string& topic, bool state);

	void reset_all_topics();

	void resubscribe();

	static void s_mqtt_async_connected(void* context, char* cause);

	static void s_mqtt_async_connection_lost(void* context, char* cause);

	static int s_mqtt_async_message_arrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);

	static void s_mqtt_async_delivery_complete(void* context, MQTTAsync_token token);


	static void s_mqtt_async_subscribe_on_success5(void* context, MQTTAsync_successData5* response);

	static void s_mqtt_async_subscribe_on_failure5(void* context, MQTTAsync_failureData5* response);


	void on_timer();

public:
	sys::callback<connected_event_t> connected_event;
	sys::callback<disconnect_event_t> disconnect_event;
	sys::callback<reconnecting_event_t> reconnecting_event;
	sys::callback<message_event_t> message_event;


private:
	mutable std::recursive_mutex mutex_;
	MQTTAsync client_ = nullptr;
	std::string uri_;
	std::string client_id_;
	std::string user_;
	std::string password_;

	std::map<std::string, topic_t> topics_;

	bool active_ = false;
	sys::signal signal_;
	std::atomic<uint32_t> interval_ = 3000;
	std::thread* timer_ = nullptr;
};

