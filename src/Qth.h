#ifndef QTH_H
#define QTH_H

#include <PubSubClient.h>
#include <Client.h>

#if MQTT_MAX_PACKET_SIZE < 512
#error "Insufficient MQTT packet size: Add build_flags = -DMQTT_MAX_PACKET_SIZE=512 (or similar) to platformio.ini"
#endif

namespace Qth {
	
	typedef void (*callback_t)(const char *topic, const char *json);
	
	const unsigned long RECONNECT_DELAY = 5000;
	
	class QthClient;
	
	class Entity {
		protected:
			const char *behaviour;
			
			const char *name;
			callback_t callback;
			const char *description;
			const char *onUnregisterJson;
			
			Entity *nextRegistration;
			Entity *nextSubscription;
			
			QthClient *qth;
			
			virtual void onConnect() {};
			
			virtual void call(const char *topic, const char *json) {
				if (callback) {
					callback(topic, json);
				}
			}
		
		public:
			Entity(const char *behaviour,
			       const char *name,
			       callback_t callback,
			       const char *description,
			       const char *onUnregisterJson) :
				behaviour(behaviour),
				name(name),
				callback(callback),
				description(description),
				onUnregisterJson(onUnregisterJson),
				nextRegistration(NULL),
				nextSubscription(NULL)
				{};
		
		friend class QthClient;
	};
	
	/**
	 * Define a Qth Property.
	 *
	 * A property is a gettable/settable long-lived, changing value, for example
	 * the state of a light or a temperature. Register with Qth, watch or set the
	 * value of the property using a QthClient.
	 */
	class Property : public Entity {
		public:
			/**
			 * Define a property with a callback on change. You must call
			 * watchProperty for changes to this property to result in the callback
			 * being called.
			 *
			 * @param name The full Qth path of the property.
			 * @param callback Callback called with the path and JSON of the
			 *        property whenever it changes.
			 * @param description A human readable description of the property.
			 *        Needn't be provided if you don't register the property.
			 * @param oneToMany Is this a one-to-many (vs many-to-one) property.
			 *        Needn't be provided if you don't register the property.
			 * @param onUnregisterJson A value to set the property to when this
			 *        client disconnects from Qth. Set to an empty string to delete
			 *        the property. Set to a valid JSON value to set it. Set to NULL
			 *        to neither set or delete the property. Needn't be provided if
			 *        you don't register the property.
			 */
			Property(const char *name,
			         callback_t callback,
			         const char *description="",
			         bool oneToMany=true,
			         const char *onUnregisterJson="") :
				Entity(oneToMany ? "PROPERTY-1:N" : "PROPERTY-N:1",
				       name, callback, description, onUnregisterJson)
				{};
			
			/**
			 * Define a property without a callback on change.
			 *
			 * This constructor is useful when your program will only set this
			 * property: without a callback it is not possible to get the value of
			 * the property (see also StoredProperty).
			 *
			 * @param name The full Qth path of the property.
			 * @param description A human readable description of the property.
			 *        Needn't be provided if you don't register the property.
			 * @param oneToMany Is this a one-to-many (vs many-to-one) property.
			 *        Needn't be provided if you don't register the property.
			 * @param onUnregisterJson A value to set the property to when this
			 *        client disconnects from Qth. Set to an empty string to delete
			 *        the property. Set to a valid JSON value to set it. Set to NULL
			 *        to neither set or delete the property. Needn't be provided if
			 *        you don't register the property.
			 */
			Property(const char *name,
			         const char *description="",
			         bool oneToMany=true,
			         const char *onUnregisterJson="") :
				Property(name, NULL, description, oneToMany, onUnregisterJson)
				{};
	};
	
	/**
	 * Define a Qth Property and store the most recent value locally (convenience
	 * API).
	 *
	 * Unlike plain Property objects, once watchProperty is called, the most
	 * recently received value of the property may be retreieved by calling
	 * get(). The initial value returned by get() before a value is received from
	 * Qth can be specified in the constructor.
	 *
	 * If the StoredProperty object is registered (with registerProperty), upon
	 * initial connection and whenever the client reconnects, the Property is
	 * automatically set to the last set value (or initial value).
	 */
	class StoredProperty : public Property {
		protected:
			char *value;
			
			void _set(const char *newValue);
			virtual void onConnect();
			virtual void call(const char *topic, const char *json);
		
		public:
			/**
			 * Define a Qth property.
			 *
			 * Though you can use setProperty to set this property, it is recommended
			 * you use the set() method of this StoredProperty object. This will
			 * ensure that calling get() will always return the latest value of the
			 * property, even while disconnected from Qth.
			 *
			 * @param name The full Qth path of the property.
			 * @param initialValue The initial value to assign to the property. If
			 *        NULL, no initial value will be set upon connection. Otherwise
			 *        this should be a vald JSON value. If the property is registered
			 *        with registerProperty, this initial value will be set upon
			 *        initial connection. Otherwise this value only represents the
			 *        initial value returned by get() before the property value is
			 *        received from the Qth server.
			 * @param description A human readable description of the property.
			 *        Needn't be provided if you don't register the property.
			 * @param oneToMany Is this a one-to-many (vs many-to-one) property.
			 *        Needn't be provided if you don't register the property.
			 * @param onUnregisterJson A value to set the property to when this
			 *        client disconnects from Qth. Set to an empty string to delete
			 *        the property. Set to a valid JSON value to set it. Set to NULL
			 *        to neither set or delete the property. Needn't be provided if
			 *        you don't register the property.
			 * @param callback Callback called with the path and JSON of the
			 *        property whenever it changes.
			 */
			StoredProperty(const char *name,
			               const char *initialValue=NULL,
			               const char *description="",
			               bool oneToMany=false,
			               const char *onUnregisterJson="",
			               callback_t callback=NULL) :
				Property(name, callback, description, oneToMany, onUnregisterJson)
			{
				_set(initialValue);
			};
			
			/**
			 * Set the value of this property.
			 *
			 * The value pased will be copied and needn't remain valid after the call
			 * returns.
			 *
			 * If this StoredProperty has been registered with Qth on this node (by
			 * registerProperty), calling set() while disconnected will result in a
			 * call to set the property once reconnected.
			 */
			void set(const char *newValue);
			
			/**
			 * Get the most recently recieved value of the property. The returned
			 * pointer may be invalidated upon the next call to any Qth API.
			 */
			const char *get();
	};
	
	/**
	 * Define a Qth Event.
	 *
	 * An event represents a transient occurrence in time. Register the event
	 * with Qth, watch or send the event using a QthClient.
	 */
	class Event : public Entity {
		public:
			/**
			 * Define an event with a callback when the event occurs. You must call
			 * watchEvent for events to result in the callback being called.
			 *
			 * @param name The full Qth path of the event.
			 * @param callback Callback called with the path and JSON of the
			 *        event.
			 * @param description A human readable description of the event.
			 *        Needn't be provided if you don't register the event.
			 * @param oneToMany Is this a one-to-many (vs many-to-one) event.
			 *        Needn't be provided if you don't register the event.
			 * @param onUnregisterJson A value to send to the event to when this
			 *        client disconnects from Qth. Set to a valid JSON value if this
			 *        is desired. Set to NULL to do nothing.
			 */
			Event(const char *name,
			      callback_t callback,
			      const char *description="",
			      bool oneToMany=true,
			      const char *onUnregisterJson=NULL) :
				Entity(oneToMany ? "EVENT-1:N" : "EVENT-N:1",
				       name, callback, description, onUnregisterJson)
				{};
			
			/**
			 * Define an event without a callback.
			 *
			 * This constructor is useful when an event is only ever sent and not
			 * watched. When used with this constructor there is no way to determine
			 * if the event is fired by another Qth client.
			 *
			 * @param name The full Qth path of the event.
			 * @param description A human readable description of the event.
			 *        Needn't be provided if you don't register the event.
			 * @param oneToMany Is this a one-to-many (vs many-to-one) event.
			 *        Needn't be provided if you don't register the event.
			 * @param onUnregisterJson A value to send to the event to when this
			 *        client disconnects from Qth. Set to a valid JSON value if this
			 *        is desired. Set to NULL to do nothing.
			 */
			Event(const char *name,
			      const char *description="",
			      bool oneToMany=true,
			      const char *onUnregisterJson=NULL) :
				Event(name, NULL, description, oneToMany, onUnregisterJson)
				{};
	};
	
	/**
	 * A client connection to Qth. Limited to one instance per application
	 * (sorry... blame PubSubClient's API).
	 *
	 * Qth properties and events may be registered, watched, sent and set via
	 * this API. Properties and events are defined by creating instances of the
	 * Property, StoredProperty and Event classes. These are then registered with
	 * Qth or watched by calling the relevant functions on your QthClient object.
	 *
	 * This implementation does not feature any JSON parsing or generation
	 * capabilities. All callbacks and API functions produce and expect raw
	 * strings containing valid JSON data. Use a 3rd party library as required.
	 */
	class QthClient {
		private:
			// Since the PubSubClient does not provide a user-supplied argument for
			// its callbacks we just assume QthClient is a singleton.
			static QthClient *qth;
			static void onMessageStatic(const char *topic, byte *payload,
			                            unsigned int length) {
				if (qth) {
					qth->onMessage(topic, (const char *)payload, length);
				}
			}
			
			
			PubSubClient mqtt;
			const char *clientId;
			const char *description;
			void (*onConnectCallback)();
			
			unsigned long lastReconnect;
			
			Entity *registrations;
			Entity *subscriptions;
			
			void onMessage(const char *topic, const char *payload, unsigned int length);
			void onConnect();
			void sendRegistration();
			
			void registerEntity(Entity *entity);
			void unregisterEntity(Entity *entity);
			
			void watchEntity(Entity *entity);
			void unwatchEntity(Entity *entity);
		
		public:
			/**
			 * Define a connection to a Qth (MQTT) server.
			 *
			 * @param mqttServer Hostname or IP of the MQTT server.
			 * @param client An Arduino network Client (e.g. a WiFiClient) for the
			 *               network connection to be used.
			 * @param clientId The unique ID of this Qth client.
			 * @param description A description of this Qth client's purpose.
			 * @param onConnectCallback A callback to call when a connection to Qth
			 *                          is (re-)made.
			 */
			QthClient(const char *mqttServer,
			          Client& client,
			          const char *clientId,
			          const char *description="",
			          void (*onConnectCallback)()=NULL) :
				mqtt(mqttServer, (uint16_t)1883, onMessageStatic, client),
				clientId(clientId),
				description(description),
				onConnectCallback(onConnectCallback),
				lastReconnect(0),
				registrations(NULL),
				subscriptions(NULL)
			{qth = this;};
			
			/**
			 * Cycle the Qth mainloop, reconnecting to Qth automatically as required.
			 * Call frequently.
			 */
			void loop();
			
			/**
			 * Is the Qth client currently connected? (The QthClient automatically
			 * reconnects as required).
			 */
			bool connected();
			
			/**
			 * Register the specified Property with Qth. (NB: Doesn't automatically
			 * watch the property, see watchProperty()).
			 */
			void registerProperty(Property *property) {registerEntity((Entity *)property);}
			
			/**
			 * Register the specified Event with Qth. (NB: Doesn't automatically
			 * watch the event, see watchEvent()).
			 */
			void registerEvent(Event *event) {registerEntity((Entity *)event);}
			
			/**
			 * Unregister the specified Property with Qth.
			 */
			void unregisterProperty(Property *property) {unregisterEntity((Entity *)property);}
			
			/**
			 * Unregister the specified Event with Qth.
			 */
			void unregisterEvent(Event *event) {unregisterEntity((Entity *)event);}
			
			/**
			 * Watch a property, calling the registered callback function when it is
			 * set.
			 */
			void watchProperty(Property *property) {watchEntity((Entity *)property);}
			
			/**
			 * Watch an event, calling the registered callback function when it is
			 * sent.
			 */
			void watchEvent(Event *event) {watchEntity((Entity *)event);}
			
			/**
			 * Stop watching a property.
			 */
			void unwatchProperty(Property *property) {unwatchEntity((Entity *)property);}
			
			/**
			 * Stop watching an event.
			 */
			void unwatchEvent(Event *event) {unwatchEntity((Entity *)event);}
			
			
			/**
			 * Set the value of a property.
			 */
			void setProperty(Property *property, const char *json);
			
			/**
			 * Send an event.
			 */
			void sendEvent(Event *event, const char *json);
	};
}

#endif
