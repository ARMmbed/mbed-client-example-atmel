/*
 * Copyright (c) 2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test_env.h"
#include "mbed-client/m2minterfacefactory.h"
#include "mbed-client/m2mdevice.h"
#include "mbed-client/m2minterfaceobserver.h"
#include "mbed-client/m2minterface.h"
#include "mbed-client/m2mobjectinstance.h"
#include "mbed-client/m2mresource.h"
#include "minar/minar.h"
#include "security.h"

#include "sal-iface-atmel-winc1500/AtWinc1500Interface.h"
#include "sal-iface-atmel-winc1500/AtWinc1500Adapter.h"

#include "common/include/nm_common.h"


using namespace mbed::util;

#define SECURE AtWinc1500Interface::AT_WIFI_SEC_WPA_PSK

#define SSID "User AP Name"
#define PASS "User AP Password"

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- "BOARD_NAME " --"STRING_EOL \
	"-- Atmel on mbed client --"STRING_EOL \
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

// Select connection mode: Certificate or NoSecurity
M2MSecurity::SecurityModeType CONN_MODE = M2MSecurity::NoSecurity;

// This is address to mbed Device Connector
const String &MBED_SERVER_ADDRESS = "coap://leshan.eclipse.org";
const int &MBED_SERVER_PORT = 5683;

const int32_t registration_life_time_s = 3600*24;
long unsigned int update_registration_life_time_ms = 40000;

const String &MBED_USER_NAME_DOMAIN = MBED_DOMAIN;
const String &ENDPOINT_NAME = MBED_ENDPOINT_NAME;

const String &MANUFACTURER = "Atmel";
const String &TYPE = "DevKit";
const String &MODEL_NUMBER = "ATSAMG55-XPRO-Beta";
const String &SERIAL_NUMBER = "0200000680";
const String &HW_VERSION = "A09-2447/02";
const String &FW_VERSION = "19.4.4";
const String &SW_VERSION = "1.0.0";
const String &MEMORY_TOTAL = "512 KB";


class MbedClient: public M2MInterfaceObserver {
public:
	MbedClient(){
		_interface = NULL;
		_bootstrapped = false;
		_error = false;
		_registered = false;
		_unregistered = false;
		_register_security = NULL;
		_value = 0;
		_object = NULL;
	}

	~MbedClient() {
		if(_interface) {
			delete _interface;
		}
		if(_register_security){
			delete _register_security;
		}
	}

	void create_interface() {
		// Creates M2MInterface using which endpoint can
		// setup its name, resource type, life time, connection mode,
		// Currently only LwIPv4 is supported.

		// Randomizing listening port for Certificate mode connectivity
		srand(time(NULL));
		uint16_t port = rand() % 65535 + 12345;

		_interface = M2MInterfaceFactory::create_interface(*this,	
															ENDPOINT_NAME, 
															"test", 
															registration_life_time_s, 
															port, 
															MBED_USER_NAME_DOMAIN, 
															M2MInterface::UDP, 
															M2MInterface::ATWINC_IPv4,  // jini.lim
															"");
	}

	bool register_successful() {
		return _registered;
	}

	bool unregister_successful() {
		return _unregistered;
	}

	M2MSecurity* create_register_object() {
		// Creates register server object with mbed device server address and other parameters
		// required for client to connect to mbed device server.
		M2MSecurity *security = M2MInterfaceFactory::create_security(M2MSecurity::M2MServer);
		if(security) {
			char buffer[6];
			sprintf(buffer,"%d",MBED_SERVER_PORT);
			m2m::String port(buffer);

			m2m::String addr = MBED_SERVER_ADDRESS;
			addr.append(":", 1);
			addr.append(port.c_str(), size_t(port.size()) );
			security->set_resource_value(M2MSecurity::M2MServerUri, addr);
			security->set_resource_value(M2MSecurity::SecurityMode, M2MSecurity::NoSecurity);
		}
		return security;
	}

	M2MDevice* create_device_object() {
		// Creates device object which contains mandatory resources linked with
		// device endpoint.
		M2MDevice *device = M2MInterfaceFactory::create_device();
		if(device) {
			device->create_resource(M2MDevice::Manufacturer,MANUFACTURER);
			device->create_resource(M2MDevice::DeviceType,TYPE);
			device->create_resource(M2MDevice::ModelNumber,MODEL_NUMBER);
			device->create_resource(M2MDevice::SerialNumber,SERIAL_NUMBER);
			device->create_resource(M2MDevice::HardwareVersion,HW_VERSION);
			device->create_resource(M2MDevice::FirmwareVersion,FW_VERSION);
			device->create_resource(M2MDevice::SoftwareVersion,SW_VERSION);
			device->create_resource(M2MDevice::MemoryTotal,MEMORY_TOTAL);
		}

		return device;
		}

	void test_register(M2MSecurity *register_object, M2MObjectList object_list){
		if(_interface) {
			// Register function
			_interface->register_object(register_object, object_list);
		}
	}

	void test_unregister() {
		if(_interface) {
			// Unregister function
			_interface->unregister_object(NULL);
		}
	}

	//Callback from mbed client stack when the bootstrap
	// is successful, it returns the mbed Device Server object
	// which will be used for registering the resources to
	// mbed Device server.
	void bootstrap_done(M2MSecurity *server_object){
		if(server_object) {
			_bootstrapped = true;
			_error = false;
			printf("\nBootstrapped\r\n");
		}
	}

	//Callback from mbed client stack when the registration
	// is successful, it returns the mbed Device Server object
	// to which the resources are registered and registered objects.
	void object_registered(M2MSecurity */*security_object*/, const M2MServer &/*server_object*/){
		_registered = true;
		_unregistered = false;
		printf("\nRegistered\r\n");
	}

	//Callback from mbed client stack when the unregistration
	// is successful, it returns the mbed Device Server object
	// to which the resources were unregistered.
	void object_unregistered(M2MSecurity */*server_object*/){
		_unregistered = true;
		_registered = false;
		notify_completion(_unregistered);
		minar::Scheduler::stop();
		printf("\nUnregistered\r\n");
	}

    void registration_updated(M2MSecurity */*security_object*/, const M2MServer & /*server_object*/){
    }

	//Callback from mbed client stack if any error is encountered
	// during any of the LWM2M operations. Error type is passed in
	// the callback.
	void error(M2MInterface::Error error){
		_error = true;
		printf("\n\rError occurred\n\r");

		if (error == M2MInterface::AlreadyExists) {
			printf("M2MInterface Error:  Already exists.\n\r");
		}

		if (error == M2MInterface::BootstrapFailed) {
			printf("M2MInterface Error:  Bootstrap failed.\n\r");
		}

		if (error == M2MInterface::InvalidParameters) {
			printf("M2MInterface Error:  Invalid parameters.\n\r");
		}

		if (error == M2MInterface::NotRegistered) {
			printf("M2MInterface Error:  Not registered.\n\r");
		}

		if (error == M2MInterface::Timeout) {
			printf("M2MInterface Error:  Timeout.\n\r");
		}

		if (error == M2MInterface::NetworkError) {
			printf("M2MInterface Error:  Network error.\n\r");
		}

		if (error == M2MInterface::ResponseParseFailed) {
			printf("M2MInterface Error: Response parse failed.\n\r");
		}

		if (error == M2MInterface::UnknownError) {
			printf("M2MInterface Error:  Unknown error.\n\r");
		}

		if (error == M2MInterface::MemoryFail) {
			printf("M2MInterface Error:  Memory fail.\n\r");
		}

		if (error == M2MInterface::NotAllowed) {
			printf("M2MInterface Error:  Not allowed.\n\r");
		}
	}

	//Callback from mbed client stack if any value has changed
	// during PUT operation. Object and its type is passed in
	// the callback.
	void value_updated(M2MBase *base, M2MBase::BaseType type) {
		printf("\nValue updated of Object name %s and Type %d\n",
		base->name().c_str(), type);
	}

	void test_update_register() {
		printf("\r\ntest_update_register\r\n");
		
		if (_registered) {
			_interface->update_registration(_register_security, 3600);
		}
	}

	void set_register_object(M2MSecurity *register_object) {
		if (_register_security == NULL) {
			_register_security = register_object;
		}
	}

	
private:
	M2MInterface    	*_interface;
	M2MSecurity         *_register_security;
	M2MObject           *_object;
	volatile bool       _bootstrapped;
	volatile bool       _error;
	volatile bool       _registered;
	volatile bool       _unregistered;
	int                 _value;
};


void app_start(int, char*[]) {
	
	// Instantiate the class which implements
	MbedClient mbed_client;

	AtWinc1500Interface winc1500;

	printf(STRING_HEADER);

	winc1500.init();
	
	if (winc1500.connect(SECURE, SSID, PASS))
	{
		printf("Failed WIFI Connection.\r\n");
		return;
	}

	// Create LWM2M Client API interface to manage register and unregister
	mbed_client.create_interface();

	// Create LWM2M server object specifying mbed device server
	// information.
	M2MSecurity* register_object = mbed_client.create_register_object();

	if( CONN_MODE == M2MSecurity::Certificate ){
		register_object->set_resource_value(M2MSecurity::SecurityMode, M2MSecurity::Certificate);
		register_object->set_resource_value(M2MSecurity::ServerPublicKey,SERVER_CERT,sizeof(SERVER_CERT));
		register_object->set_resource_value(M2MSecurity::PublicKey,CERT,sizeof(CERT));
		register_object->set_resource_value(M2MSecurity::Secretkey,KEY,sizeof(KEY));
	} else{
		register_object->set_resource_value(M2MSecurity::SecurityMode, M2MSecurity::NoSecurity);
	}

	// Create LWM2M device object specifying device resources
	// as per OMA LWM2M specification.
	M2MDevice* device_object = mbed_client.create_device_object();

	// Add all the objects that you would like to register
	// into the list and pass the list for register API.
	M2MObjectList object_list;
	object_list.push_back(device_object);

	mbed_client.set_register_object(register_object);

	// Issue register command.
	FunctionPointer2<void, M2MSecurity*, M2MObjectList> fp(&mbed_client, &MbedClient::test_register);
	minar::Scheduler::postCallback(fp.bind(register_object,object_list));

	minar::Scheduler::postCallback(&mbed_client, &MbedClient::test_update_register).
		delay(minar::milliseconds(update_registration_life_time_ms)).
		period(minar::milliseconds(update_registration_life_time_ms));

	minar::Scheduler::start();

	// Delete device object created for registering device
	// resources.
	if(device_object) {
		M2MDevice::delete_instance();
	}

	// Disconnect the connect and teardown the network interface
	winc1500.disconnect();

}

