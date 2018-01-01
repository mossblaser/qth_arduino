#include "Qth.h"

void Qth::StoredProperty::_set(const char *newValue) {
	char *oldValue = value;
	
	if (newValue) {
		size_t len = strlen(newValue);
		value = (char *)malloc(len + 1);
		memcpy(value, newValue, len + 1);
	} else {
		value = NULL;
	}
	
	// NB: Free afterwards incase someone sets the property to its current
	// value!
	if (oldValue) {
		free(oldValue);
	}
}

void Qth::StoredProperty::set(const char *newValue) {
	_set(newValue);
	if (qth && value) {
		qth->setProperty(this, value);
	}
	call(name, value);
}

const char *Qth::StoredProperty::get() {
	return value;
}

void Qth::StoredProperty::call(const char *topic, const char *json) {
	_set(json);
	Property::call(topic, json);
}


void Qth::StoredProperty::onConnect() {
	set(value);
	Property::onConnect();
};


Qth::QthClient *Qth::QthClient::qth = NULL;

void Qth::QthClient::loop() {
	// Reconnect if required
	if (!mqtt.connected()) {
		unsigned long now = millis();
		if (now - lastReconnect > Qth::RECONNECT_DELAY) {
			lastReconnect = now;
			
			size_t topicLen = strlen("meta/clients/") + strlen(clientId) + 1;
			char lwtTopic[topicLen];
			sprintf(lwtTopic, "meta/clients/%s", clientId);
			
			int lwtQoS = 2;
			bool lwtRetain = true;
			const char lwtMessage[] = "";
			
			if (mqtt.connect(clientId, lwtTopic, lwtQoS, lwtRetain, lwtMessage)) {
				onConnect();
			}
		}
	}
	
	mqtt.loop();
}

bool Qth::QthClient::connected() {
	return mqtt.connected();
}

void Qth::QthClient::onMessage(const char *topic, const char *payload, unsigned int length) {
	Qth::Entity *subscription = subscriptions;
	while (subscription) {
		if (strcmp(subscription->name, topic) == 0) {
			// Make a null-terminated copy of the payload on the stack
			char payloadNullTerminated[length];
			memcpy(payloadNullTerminated, payload, length);
			payloadNullTerminated[length] = 0;
			
			subscription->call(topic, payloadNullTerminated);
		}
		
		subscription = subscription->nextSubscription;
	}
}

void Qth::QthClient::sendRegistration() {
	const char *REG_PRE_TMPL = "{\"description\":\"%s\",\"topics\":{";
	const char *REG_POST_TMPL = "}}";
	const char *TOPIC_TMPL = "\"%s\":{\"description\":\"%s\",\"behaviour\":\"%s\"},";
	const char *TOPIC_ON_UNREGISTER_TMPL = "\"%s\":{\"description\":\"%s\",\"behaviour\":\"%s\",\"on_unregister\":%s},";
	const char *topic_delete_on_unregister_tmpl = "\"%s\":{\"description\":\"%s\",\"behaviour\":\"%s\",\"delete_on_unregister\":true},";
	
	// Work out length of registration string
	size_t regLen = strlen(REG_PRE_TMPL) - (1 * 2);
	regLen += strlen(description);
	Qth::Entity *entity = registrations;
	while (entity) {
		if (entity->onUnregisterJson == NULL) {
			regLen += strlen(TOPIC_TMPL) - (3 * 2);
		} else if (strlen(entity->onUnregisterJson) == 0) {
			regLen += strlen(topic_delete_on_unregister_tmpl) - (3 * 2);
		} else {
			regLen += strlen(TOPIC_ON_UNREGISTER_TMPL) - (4 * 2);
			regLen += strlen(entity->onUnregisterJson);
			
		}
		regLen += strlen(entity->name);
		regLen += strlen(entity->description);
		regLen += strlen(entity->behaviour);
		
		entity = entity->nextRegistration;
	}
	regLen -= 1; // Drop trailing comma
	regLen += strlen(REG_POST_TMPL);
	regLen += 1; // Add null terminator
	
	// Generate the registration JSON
	char *outBuf = (char *)malloc(regLen);
	char *cursor = outBuf;
	cursor += sprintf(cursor, REG_PRE_TMPL, description);
	
	entity = registrations;
	while (entity) {
		if (entity->onUnregisterJson == NULL) {
			cursor += sprintf(cursor, TOPIC_TMPL,
			                  entity->name,
			                  entity->description,
			                  entity->behaviour);
		} else if (strlen(entity->onUnregisterJson) == 0) {
			cursor += sprintf(cursor, topic_delete_on_unregister_tmpl,
			                  entity->name,
			                  entity->description,
			                  entity->behaviour);
		} else {
			cursor += sprintf(cursor, TOPIC_ON_UNREGISTER_TMPL,
			                  entity->name,
			                  entity->description,
			                  entity->behaviour,
			                  entity->onUnregisterJson);
		}
		
		entity = entity->nextRegistration;
	}
	if (registrations) {
		// Drop trailing comma
		cursor--;
	}
	
	cursor += sprintf(cursor, REG_POST_TMPL);
	
	size_t topicLen = strlen("meta/clients/") + strlen(clientId) + 1;
	char topic[topicLen];
	sprintf(topic, "meta/clients/%s", clientId);
	
	mqtt.publish(topic, outBuf, true);
	
	free(outBuf);
}

void Qth::QthClient::onConnect() {
	sendRegistration();
	
	// Run on-connection logic for all registered values (e.g. to send initial
	// values or most recent values when reconnecting).
	Qth::Entity *entity = registrations;
	while (entity) {
		entity->onConnect();
		entity = entity->nextRegistration;
	}
	
	// Set up all existing subscriptions.
	entity = subscriptions;
	while (entity) {
		mqtt.subscribe(entity->name, 1);  // QoS 2 not available
		entity = entity->nextSubscription;
	}
	
	// User callback
	if (onConnectCallback) {
		onConnectCallback();
	}
}

void Qth::QthClient::registerEntity(Qth::Entity *entity) {
	// Insert into list
	entity->nextRegistration = registrations;
	registrations = entity;
	
	// Simulate connection (if already connected)
	if (connected()) {
		entity->onConnect();
	}
	
	entity->qth = this;
	
	sendRegistration();
}

void Qth::QthClient::unregisterEntity(Qth::Entity *entity) {
	// Remove from list
	Qth::Entity **registrationPtr = &registrations;
	while (*registrationPtr) {
		if ((*registrationPtr) == entity) {
			*registrationPtr = entity->nextRegistration;
		}
		registrationPtr = &((*registrationPtr)->nextRegistration);
	}
	
	sendRegistration();
}

void Qth::QthClient::watchEntity(Qth::Entity *entity) {
	// Insert into list
	entity->nextSubscription = subscriptions;
	subscriptions = entity;
	
	entity->qth = this;
	
	mqtt.subscribe(entity->name, 1);  // QoS 2 not available
}

void Qth::QthClient::unwatchEntity(Qth::Entity *entity) {
	// Remove from list
	Qth::Entity **subscriptionPtr = &subscriptions;
	while (*subscriptionPtr) {
		if ((*subscriptionPtr) == entity) {
			*subscriptionPtr = entity->nextSubscription;
		}
		subscriptionPtr = &((*subscriptionPtr)->nextSubscription);
	}
	
	mqtt.unsubscribe(entity->name);
}

void Qth::QthClient::setProperty(Property *property, const char *json) {
	mqtt.publish(property->name, json, true);
}

void Qth::QthClient::sendEvent(Event *event, const char *json) {
	mqtt.publish(event->name, json, false);
}


