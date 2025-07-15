#include "mqtt_client.h"
#include <sys2/string_util.h>

mqtt_client::mqtt_client()
{

}

mqtt_client::~mqtt_client()
{
	stop();
}

bool mqtt_client::start(const std::string& uri, const std::string& client_id, const std::string& user, const std::string& password)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	if (active_)
	{
		return false;
	}
	active_ = true;

	reconnect(uri, client_id, user, password);
	timer_ = new std::thread(&mqtt_client::on_timer, this);
	return true;
}

void mqtt_client::stop()
{
	{
		std::lock_guard<std::recursive_mutex> lk(mutex_);
		active_ = false;

		if (client_ != nullptr) {
			MQTTAsync_destroy(&client_);
			client_ = nullptr;
		}
		topics_.clear();
	}

	signal_.notify();
	if (timer_)
	{
		timer_->join();
		delete timer_;
		timer_ = nullptr;
	}

}

bool mqtt_client::reconnect(const std::string& uri, const std::string& client_id, const std::string& user, const std::string& password)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	uri_ = uri;
	client_id_ = client_id;
	user_ = user;
	password_ = password;

	MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer5;
	int rc = MQTTAsync_createWithOptions(&client_, uri_.c_str(), client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
	MQTTAsync_setCallbacks(client_, this, s_mqtt_async_connection_lost, s_mqtt_async_message_arrived, s_mqtt_async_delivery_complete);
	MQTTAsync_setConnected(client_, this, s_mqtt_async_connected);


	MQTTAsync_SSLOptions ssl = MQTTAsync_SSLOptions_initializer;
	ssl.enableServerCertAuth = 0;

	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer5;
	conn_opts.ssl = &ssl;
	conn_opts.automaticReconnect = 0;
	conn_opts.cleansession = 0;
	conn_opts.connectTimeout = 10;
	conn_opts.keepAliveInterval = 30;
	conn_opts.minRetryInterval = 3;
	conn_opts.maxRetryInterval = 3;
	conn_opts.username = user.c_str();
	conn_opts.password = password.c_str();
	conn_opts.cleanstart = 0;
	//conn_opts.onSuccess5 = s_mqtt_async_on_success5;
	//conn_opts.onFailure5 = s_mqtt_async_on_failure5;
	conn_opts.context = this;
	MQTTProperties ps = MQTTProperties_initializer;
	conn_opts.connectProperties = &ps;
	MQTTProperty p;
	p.identifier = MQTTPropertyCodes::MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL;
	p.value.integer4 = 350;
	MQTTProperties_add(conn_opts.connectProperties, &p);
	rc = MQTTAsync_connect(client_, &conn_opts);
	MQTTProperties_free(conn_opts.connectProperties);
	if (rc != MQTTASYNC_SUCCESS) {
		return false;
	}

	return true;
}


bool mqtt_client::subscribe(const std::string& topic, int qos)
{
	topic_t tp = {};
	tp.qos = qos;
	tp.topic = topic;
	tp.state = false;
	topics_.insert(std::make_pair(tp.topic, tp));
	return _subscribe(tp);
}

void mqtt_client::unsubscribe(const std::string& topic)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);

	auto iter = topics_.find(topic);
	if (iter != topics_.end())
	{
		topics_.erase(iter);
	}

	if (client_) {
		MQTTAsync_unsubscribe(client_, topic.c_str(), nullptr);
	}

}

void mqtt_client::clear_subscribes()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);

	std::vector<char*> vec;
	for (auto itr = topics_.begin(); itr != topics_.end(); itr++)
	{
		vec.push_back(sys::string_util::alloc(itr->second.topic.c_str()));
	}
	topics_.clear();

	if (client_) {
		MQTTAsync_unsubscribeMany(client_, (int)vec.size(), vec.data(), nullptr);
	}

	for (auto itr = vec.begin(); itr != vec.end(); itr++)
	{
		sys::string_util::free(*itr);
	}
}

bool mqtt_client::publish(const std::string& topic, const std::string& message, int qos, bool retained)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	if (!client_) {
		return false;
	}
	int r = MQTTAsync_send(client_, topic.c_str(), (int)message.size(), message.data(), qos, retained ? 1 : 0, nullptr);
	if (r != MQTTASYNC_SUCCESS) {
		return false;
	}
	return true;
}

bool mqtt_client::is_connected()const
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	if (!client_)
	{
		return false;
	}
	return MQTTAsync_isConnected(client_);

}

bool mqtt_client::_subscribe(topic_t& tp)
{
	tp.state = false;
	if (!client_)
	{
		return false;
	}

	sub_context_t* subctx = new sub_context_t();
	subctx->ctx = this;
	subctx->topic = tp.topic;

	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	opts.context = subctx;
	opts.onSuccess5 = s_mqtt_async_subscribe_on_success5;
	opts.onFailure5 = s_mqtt_async_subscribe_on_failure5;
	opts.struct_version = 1;

	int r = MQTTAsync_subscribe(client_, tp.topic.c_str(), tp.qos, &opts);
	if (r != MQTTASYNC_SUCCESS) {
		delete subctx;
		return false;
	}

	return true;
}

bool mqtt_client::set_topic_state(const std::string& topic, bool state)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	auto itr = topics_.find(topic);
	if (itr == topics_.end())
	{
		return false;
	}

	itr->second.state = state;
	return true;
}

void mqtt_client::reset_all_topics()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	for (auto itr = topics_.begin(); itr != topics_.end(); itr++)
	{
		itr->second.state = false;
	}
}

void mqtt_client::resubscribe()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	for (auto itr = topics_.begin(); itr != topics_.end(); itr++)
	{
		if (!itr->second.state)
		{
			_subscribe(itr->second);
		}
	}
}

void mqtt_client::s_mqtt_async_connected(void* context, char* cause)
{
	mqtt_client* p = (mqtt_client*)context;
	p->resubscribe();
	p->connected_event.invoke();
}

void mqtt_client::s_mqtt_async_connection_lost(void* context, char* cause)
{
	mqtt_client* p = (mqtt_client*)context;
	p->signal_.notify();
	p->disconnect_event.invoke();
}

int mqtt_client::s_mqtt_async_message_arrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
	mqtt_client* p = (mqtt_client*)context;

	std::string topic, payload;
	topic.assign(topicName, topicLen);
	payload.assign((const char*)message->payload, message->payloadlen);
	p->message_event.invoke(topic, payload, message->qos);

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);


	return 1;
}

void mqtt_client::s_mqtt_async_delivery_complete(void* context, MQTTAsync_token token)
{

}


void mqtt_client::s_mqtt_async_subscribe_on_success5(void* context, MQTTAsync_successData5* response)
{
	sub_context_t* subctx = (sub_context_t*)context;
	mqtt_client* p = (mqtt_client*)subctx->ctx;
	p->set_topic_state(subctx->topic, true);
	delete subctx;
}

void mqtt_client::s_mqtt_async_subscribe_on_failure5(void* context, MQTTAsync_failureData5* response)
{
	sub_context_t* subctx = (sub_context_t*)context;
	mqtt_client* p = (mqtt_client*)subctx->ctx;
	p->set_topic_state(subctx->topic, false);
	delete subctx;
}


void mqtt_client::on_timer()
{
	while (active_)
	{
		signal_.wait(interval_);
		if (is_connected())
		{
			if (interval_ < 60000)
			{
				interval_ += 1000;
			}

			resubscribe();
			continue;
		}

		reset_all_topics();
		interval_ = 3000;

		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (client_ != nullptr) {
				MQTTAsync_destroy(&client_);
				client_ = nullptr;
			}
		}
		reconnecting_event.invoke(uri_, client_id_, user_, password_);
		reconnect(uri_.c_str(), client_id_.c_str(), user_.c_str(), password_.c_str());
	}
}

