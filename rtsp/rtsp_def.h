/**
 * @file rtsp_def.h
 * @brief .
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

typedef enum _rtsp_transport_type_t
{
	rtsp_transport_type_tcp,
	rtsp_transport_type_udp,
}rtsp_transport_type_t;


typedef enum _rtsp_mode_t
{
	rtsp_mode_unknown=0,
	rtsp_mode_push=1,
	rtsp_mode_pull=2,
}rtsp_mode_t;


