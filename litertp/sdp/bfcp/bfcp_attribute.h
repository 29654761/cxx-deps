#pragma once
#include <string>


enum class bfcp_attribute_enum_t:uint8_t
{
	unknown=0,
	beneficiary_id = 1,					//U16
	floor_id = 2,						//U16
	floor_request_id = 3,				//U16
	priority = 4,						//String16
	request_status = 5,					//String16
	error_code = 6,						//String
	error_info = 7,						//String
	participant_provided_info = 8,		//String
	status_info = 9,					//String
	supported_attributes = 10,			//String
	supported_primitives = 11,			//String
	user_display_name = 12,				//String
	user_uri = 13,						//String
	beneficiary_information = 14,
	floor_request_information = 15,
	requested_by_information = 16,
	floor_request_status = 17,
	overall_request_status = 18,
};

class bfcp_attribute
{
public:
	bfcp_attribute();
	~bfcp_attribute();

	int deserialize(const uint8_t* data,int len);
	std::string serialize()const;

	void set_ushort(uint16_t v);
	uint16_t get_ushort()const;

	void set_string(const std::string& v);
	std::string get_string()const;

	void set_attribute(const bfcp_attribute& attr);
	bool get_attribute(bfcp_attribute& attr)const;
public:
	bfcp_attribute_enum_t type : 7;
	uint8_t m : 1;
	std::string contents;
};

