/*-------------------------------------------------------------

Copyright (C) 2008-2026
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Zarithya
Alberto Mardegan (mardy)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include "worker.h"

#include "device.h"
#include "ir.h"
#include "motion_plus.h"
#include "speaker.h"

#include "conf.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/client.h"
#include "bt-embedded/l2cap_server.h"
#include "bt-embedded/services/hid.h"
#include "tuxedo/mailbox.h"
#include "tuxedo/thread.h"

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#define MAX_INQUIRY_RESPONSES 8
#define CONF_PAD_MAX_ACTIVE 4
#define MAX_LINK_KEYS WPAD2_MAX_DEVICES

#define WPAD2_VENDOR_EVENT_PAIRING     0x08
#define WPAD2_VENDOR_EVENT_WIPE_PAIRED 0x09

static struct {
	int num_responses;
	int num_names_queried;
	BteHciInquiryResponse responses[MAX_INQUIRY_RESPONSES];
} *s_inquiry_responses = NULL;

static BteClient *s_client;
static BteL2capServer *s_l2cap_server_hid_ctrl;
static BteL2capServer *s_l2cap_server_hid_intr;
static enum {
	WPAD_PAIR_MODE_NORMAL,
	WPAD_PAIR_MODE_TEMPORARY,
} s_pair_mode;
static BteBdAddr s_local_address;
static int8_t s_num_stored_keys = -1; /* Not retrieved */
static BteHciStoredLinkKey s_stored_keys[MAX_LINK_KEYS];

static BtePacketType s_packet_types = 0;
static char s_nintendo_rvl[] = "Nintendo RVL-";

static conf_pads s_wpad_paired = {0};
static conf_pad_guests s_wpad_guests = {0};

#define WORKER_THREAD_STACK_SIZE (4 * 1024)
static KThread s_worker_thread;
static uint8_t *s_worker_thread_stack = NULL;
static bool s_worker_thread_done = false;
static WpadEventCb s_worker_thread_event_cb;

/* Incoming commands. */
#define WORKER_CMD_OPCODE_MASK 0x0000FFFF
#define WORKER_CMD_PARAM_MASK  0xFFFF0000
#define WORKER_CMD_OPCODE(cmd) ((cmd) & WORKER_CMD_OPCODE_MASK)
#define WORKER_CMD_PARAMS(cmd) ((cmd) >> 16)
#define WORKER_CMD(opcode, params) (WORKER_CMD_##opcode | ((params) << 16))
enum {
	WORKER_CMD_SET_SEARCH,
	WORKER_CMD_SET_FORMAT,
	WORKER_CMD_SET_MOTION_PLUS,
	WORKER_CMD_SET_RUMBLE,
	WORKER_CMD_SET_SPEAKER,
	WORKER_CMD_PLAY_SOUND,
	WORKER_CMD_DISCONNECT,
	WORKER_CMD_WIPE_CONTROLLERS,
	WORKER_CMD_START_PAIRING,
};
static KMailbox s_mailbox_in;
static uptr s_mailbox_in_slots[16];

static uint32_t s_timer_period = 0;
static uint64_t s_timer_last_trigger;
static int s_timer_clients = 0;

static inline uint64_t get_time_us()
{
	return PPCTicksToUs(PPCGetTickCount());
}

static inline bool bd_address_is_equal(const BteBdAddr *dest,
                                       const BteBdAddr *src)
{
	return memcmp(dest, src, sizeof(BteBdAddr)) == 0;
}

static inline void bd_address_copy(BteBdAddr *dest, const BteBdAddr *src)
{
	*dest = *src;
}

static inline void bd_address_to_conf(const BteBdAddr *a, u8 *conf)
{
	const uint8_t *b = a->bytes;
	conf[0] = b[5];
	conf[1]	= b[4];
	conf[2]	= b[3];
	conf[3] = b[2];
	conf[4] = b[1];
	conf[5] = b[0];
}

static inline BteBdAddr bd_address_from_conf(const u8 *conf)
{
	BteBdAddr addr = {{ conf[5], conf[4], conf[3], conf[2], conf[1], conf[0] }};
	return addr;
}

static void query_name_next(BteHci *hci);

static inline bool is_initialized()
{
	return s_num_stored_keys >= 0;
}

static WpadDevice *wpad_device_from_addr(const BteBdAddr *address)
{
	for (int i = 0; i < WPAD2_MAX_DEVICES; i++) {
		WpadDevice *device = _wpad2_device_get(i);
		if (bd_address_is_equal(address, &device->address)) {
			return device;
		}
	}
	return NULL;
}

static bool wpad_device_is_pairing(const BteBdAddr *address)
{
	WpadDevice *device = wpad_device_from_addr(address);
	return device ? (device->hid_ctrl == NULL) : false;
}

static void device_event_connected(WpadDevice *device)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_CONNECTED;
	s_worker_thread_event_cb(&event);
}

static int GetActiveSlot(const BteBdAddr *pad_addr)
{
	for (int i = 0; i < CONF_PAD_MAX_ACTIVE; i++) {
		BteBdAddr bdaddr = bd_address_from_conf(s_wpad_paired.active[i].bdaddr);
		if (bd_address_is_equal(pad_addr, &bdaddr)) {
			return i;
		}
	}

	return -1;
}

static void process_handshake(WpadDevice *device, uint8_t param,
                              BteBufferReader *reader)
{
	WPAD2_DEBUG("param %02x, length: %d", param, reader->packet->size - reader->pos_in_packet);
}

static void process_data(WpadDevice *device, uint8_t param,
                         BteBufferReader *reader)
{
	WPAD2_DEBUG("param %02x, length: %d", param, reader->packet->size - reader->pos_in_packet);
	if (param != BTE_HID_REP_TYPE_INPUT) {
		/* Ignore */
		return;
	}

	/* Here it's as being in __wiiuse_receive */
	uint8_t buffer[MAX_PAYLOAD] = { 0, };
	uint16_t len = bte_buffer_reader_read(reader, buffer, sizeof(buffer));
	if (len < 3) return;

	_wpad2_device_event(device, buffer, len);
}

static void message_received_cb(BteL2cap *l2cap, BteBufferReader *reader,
                                void *userdata)
{
	WpadDevice *device = userdata;
	uint8_t *hdr_ptr = bte_buffer_reader_read_n(reader, 1);
	if (!hdr_ptr) return;

	uint8_t hdr = *hdr_ptr;
	uint8_t type = hdr & BTE_HID_HDR_TRANS_MASK;
	uint8_t param = hdr & BTE_HID_HDR_PARAM_MASK;
	switch (type) {
	case BTE_HID_TRANS_HANDSHAKE:
		process_handshake(device, param, reader);
		break;
	case BTE_HID_TRANS_DATA:
		process_data(device, param, reader);
		break;
	default:
		WPAD2_DEBUG("got transaction %02x", type);
	}
}

static void device_set_active(WpadDevice *device)
{
	int slot = _wpad2_device_get_slot(device);
	uint8_t conf_addr[6];
	bd_address_to_conf(&device->address, conf_addr);
	uint8_t *dest = s_wpad_paired.active[slot].bdaddr;
	if (memcmp(dest, conf_addr, sizeof(conf_addr)) != 0) {
		memcpy(dest, conf_addr, sizeof(conf_addr));
		CONF_SetPadDevices(&s_wpad_paired);
		CONF_SaveChanges();
	}
}

static void device_register_paired(const BteBdAddr *address, const char *name)
{
	/* Store the key on the guest CONF data */
	int slot = -1;
	uint8_t conf_addr[6];
	bd_address_to_conf(address, conf_addr);
	for (int i = 0; i < s_wpad_paired.num_registered; i++) {
		if (memcmp(conf_addr, s_wpad_paired.registered[i].bdaddr, 6) == 0) {
			slot = i;
			break;
		}
	}

	/* move the slots so that this gets into the first position, since we
	 * want the most recent devices first */
	int n_move = 0;
	if (slot > 0) {
		n_move = slot;
	} else if (slot < 0) {
		if (s_wpad_paired.num_registered < CONF_PAD_MAX_REGISTERED) {
			n_move = s_wpad_paired.num_registered;
			s_wpad_paired.num_registered++;
		} else {
			n_move = CONF_PAD_MAX_REGISTERED - 1;
		}
	}

	if (n_move > 0) {
		memmove(&s_wpad_paired.registered[1],
		        &s_wpad_paired.registered[0],
		        sizeof(conf_pad_device) * n_move);
	}

	memcpy(s_wpad_paired.registered[0].bdaddr, conf_addr, 6);
	const int name_size = sizeof(s_wpad_paired.registered[0].name);
	strncpy(s_wpad_paired.registered[0].name, name, name_size);
	/* Make sure the string is zero-terminated */
	s_wpad_paired.registered[0].name[name_size - 1] = '\0';
}

static void device_register_guest(const BteBdAddr *address, const char *name)
{
	/* Store the key on the guest CONF data */
	int slot = -1;
	uint8_t conf_addr[6];
	bd_address_to_conf(address, conf_addr);
	for (int i = 0; i < s_wpad_guests.num_guests; i++) {
		if (memcmp(conf_addr, s_wpad_guests.guests[i].bdaddr, 6) == 0) {
			slot = i;
			break;
		}
	}

	/* move the slots so that this gets into the first position, since we
	 * want the most recent guests first */
	int n_move = 0;
	if (slot > 0) {
		n_move = slot;
	} else if (slot < 0) {
		if (s_wpad_guests.num_guests < CONF_PAD_MAX_ACTIVE) {
			n_move = s_wpad_guests.num_guests;
			s_wpad_guests.num_guests++;
		} else {
			n_move = CONF_PAD_MAX_ACTIVE - 1;
		}
	}

	if (n_move > 0) {
		memmove(&s_wpad_guests.guests[1],
		        &s_wpad_guests.guests[0],
		        sizeof(conf_pad_guest_device) * n_move);
	}

	memcpy(s_wpad_guests.guests[0].bdaddr, conf_addr, 6);
	const int name_size = sizeof(s_wpad_guests.guests[0].name);
	strncpy(s_wpad_guests.guests[0].name, name, name_size);
	/* Make sure the string is zero-terminated */
	s_wpad_guests.guests[0].name[name_size - 1] = '\0';
}

static conf_pad_device *device_get_paired(const BteBdAddr *address)
{
	uint8_t conf_addr[6];
	bd_address_to_conf(address, conf_addr);
	for (int i = 0; i < s_wpad_paired.num_registered; i++) {
		if (memcmp(conf_addr, s_wpad_paired.registered[i].bdaddr, 6) == 0) {
			return &s_wpad_paired.registered[i];
		}
	}
	return NULL;
}

static conf_pad_guest_device *device_get_guest(const BteBdAddr *address)
{
	uint8_t conf_addr[6];
	bd_address_to_conf(address, conf_addr);
	for (int i = 0; i < s_wpad_guests.num_guests; i++) {
		if (memcmp(conf_addr, s_wpad_guests.guests[i].bdaddr, 6) == 0) {
			return &s_wpad_guests.guests[i];
		}
	}
	return NULL;
}

static void on_channels_connected(WpadDevice *device)
{
	/* The BT-HID header tells us which packet type we are reading,
	 * therefore we can assing the same callback to both channels */
	bte_l2cap_on_message_received(device->hid_ctrl, message_received_cb);
	bte_l2cap_on_message_received(device->hid_intr, message_received_cb);

	device_set_active(device);

	device_event_connected(device);

	device->state = STATE_HANDSHAKE_LEDS_OFF;
	_wpad2_device_set_leds(device, WIIMOTE_LED_NONE);
	_wpad2_device_request_status(device);
}

static void hid_intr_state_changed_cb(BteL2cap *l2cap, BteL2capState state,
                                      void *userdata)
{
	WpadDevice *device = userdata;

	printf("%s: state %d\n", __func__, state);
	if (state == BTE_L2CAP_OPEN) {
		on_channels_connected(device);
	}
}

static void l2cap_configure_cb(
	BteL2cap *l2cap, const BteL2capConfigureReply *reply, void *userdata)
{
	printf("%s: rejected mask: %08x\n", __func__, reply->rejected_mask);
	if (reply->rejected_mask != 0) {
		bte_l2cap_disconnect(l2cap);
	}
}

static void device_free(WpadDevice *device)
{
	BteL2cap *hid_ctrl = device->hid_ctrl;
	device->hid_ctrl = NULL;
	BteL2cap *hid_intr = device->hid_intr;
	device->hid_intr = NULL;
	if (hid_ctrl) bte_l2cap_unref(hid_ctrl);
	if (hid_intr) bte_l2cap_unref(hid_intr);
	if (device->last_read_data) {
		free(device->last_read_data);
		device->last_read_data = NULL;
	}
}

static void disconnect_notify_and_free(WpadDevice *device, uint8_t reason)
{
	int slot = _wpad2_device_get_slot(device);
	conf_pad_device *active_slot = &s_wpad_paired.active[slot];
	/* Just to be extra safe, check that the address in the active devices list
	 * matches */
	BteBdAddr conf_address = bd_address_from_conf(active_slot->bdaddr);
	if (bd_address_is_equal(&device->address, &conf_address)) {
		memset(active_slot, 0, sizeof(*active_slot));
		CONF_SetPadDevices(&s_wpad_paired);
		CONF_SaveChanges();
	}

	WpadEvent event;
	event.channel = slot;
	event.type = WPAD2_EVENT_TYPE_DISCONNECTED;
	/* WPAD_DISCON_* reasons map 1:1 to HCI errors */
	event.u.disconnection_reason = reason;
	s_worker_thread_event_cb(&event);

	device_free(device);
}

static void acl_disconnected_cb(BteL2cap *l2cap, uint8_t reason, void *userdata)
{
	WpadDevice *device = userdata;

	WPAD2_DEBUG("reason %02x", reason);
	disconnect_notify_and_free(device, reason);
}

static void l2cap_disconnected_cb(BteL2cap *l2cap, uint8_t reason, void *userdata)
{
	WPAD2_DEBUG("reason %x", reason);
}

static void device_hid_intr_connected(BteL2cap *l2cap)
{
	WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
	if (!device) return; /* Should never happen */

	device->hid_intr = bte_l2cap_ref(l2cap);
	bte_l2cap_set_userdata(l2cap, device);
	/* Default configuration parameters are fine */
	bte_l2cap_configure(l2cap, NULL, l2cap_configure_cb, NULL);
	bte_l2cap_on_state_changed(l2cap, hid_intr_state_changed_cb);
	bte_l2cap_on_disconnected(l2cap, l2cap_disconnected_cb, device);
}

static void hid_intr_connect_cb(
	BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata)
{
	printf("%s: result %d, status %d\n", __func__, reply->result, reply->status);

	if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK) {
		return;
	}

	WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
	if (!device) return; /* Should never happen */

	device_hid_intr_connected(l2cap);
}

static void hid_ctrl_state_changed_cb(BteL2cap *l2cap, BteL2capState state,
                                      void *userdata)
{
	WpadDevice *device = userdata;
	printf("%s: state %d\n", __func__, state);
	if (state == BTE_L2CAP_OPEN) {
		/* Connect the HID data channel. Since the ACL is already there, the
		 * connection parameters are ignored. */
		bte_l2cap_new_outgoing(s_client, &device->address, BTE_L2CAP_PSM_HID_INTR,
		                       NULL, 0, hid_intr_connect_cb, NULL);
	}
}

static void device_hid_ctrl_connected(BteL2cap *l2cap)
{
	WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
	if (!device) return; /* Should never happen */

	device->hid_ctrl = bte_l2cap_ref(l2cap);
	bte_l2cap_set_userdata(l2cap, device);
	/* Default configuration parameters are fine */
	bte_l2cap_configure(l2cap, NULL, l2cap_configure_cb, NULL);
	bte_l2cap_on_acl_disconnected(l2cap, acl_disconnected_cb);
}

static void hid_ctrl_connect_cb(
	BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata)
{
	WpadDevice *device = userdata;

	WPAD2_DEBUG("result %d, status %d", reply->result, reply->status);

	if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK) {
		return;
	}

	device_hid_ctrl_connected(l2cap);
	bte_l2cap_on_state_changed(l2cap, hid_ctrl_state_changed_cb);
	bte_l2cap_on_disconnected(l2cap, l2cap_disconnected_cb, device);
}

static void device_init(WpadDevice *device, const BteBdAddr *address)
{
	/* Save the information that should persist, and restore it after the memset */
	bool accel_requested = device->accel_requested;
	bool ir_requested = device->ir_requested;
	bool speaker_requested = device->speaker_requested;
	bool motion_plus_requested = device->motion_plus_requested;
	memset(device, 0, sizeof(*device));
	bd_address_copy(&device->address, address);
	device->unid = _wpad2_device_get_slot(device);
	device->speaker_volume = 0x40;
	device->ir_sensor_level = 3;

	device->accel_requested = accel_requested;
	device->ir_requested = ir_requested;
	device->speaker_requested = speaker_requested;
	device->motion_plus_requested = motion_plus_requested;
}

static WpadDevice *device_allocate(const BteBdAddr *address)
{
	for (int slot = 0; slot < WPAD2_MAX_DEVICES; slot++) {
		WpadDevice *device = _wpad2_device_get(slot);
		if (device->hid_ctrl == NULL) {
			device_init(device, address);
			return device;
		}
	}

	return NULL;
}

static bool add_new_device(BteHci *hci, const BteHciInquiryResponse *info,
                           const char *name)
{
	if (strncmp(name, s_nintendo_rvl, sizeof(s_nintendo_rvl) - 1) != 0) {
		return false;
	}

	int slot = -1;

	/* Found Wii accessory, is it controller or something else? */
	const char *suffix = name + sizeof(s_nintendo_rvl) - 1;
	if (strncmp(suffix, "CNT-", 4) == 0) {
		/* It's an ordinary controller */
		slot = GetActiveSlot(&info->address);
		if (slot >= 0) {
			WPAD2_DEBUG("Already active in slot %d", slot);
		} else {
			/* Get a free slot */
			BteBdAddr null_addr = { 0, };
			WpadDevice *device = wpad_device_from_addr(&null_addr);
			if (device) {
				slot = _wpad2_device_get_slot(device);
			}
		}
	} else {
		/* Assume balance board */
		WpadDevice *device = _wpad2_device_get(WPAD_BALANCE_BOARD);
		BteBdAddr null_addr = { 0, };
		if (bd_address_is_equal(&device->address, &info->address) ||
			bd_address_is_equal(&device->address, &null_addr)) {
			slot = WPAD_BALANCE_BOARD;
		}
	}

	if (slot < 0) {
		WPAD2_WARNING("all device slots used");
		return false;
	}

	WpadDevice *device = _wpad2_device_get(slot);
	device_init(device, &info->address);

	if (s_pair_mode == WPAD_PAIR_MODE_NORMAL) {
		device_register_paired(&info->address, name);
		/* Don't store the CONF now, that will happen in device_set_active() */
	} else {
		device_register_guest(&info->address, name);
		/* Don't store the CONF now, we'll do that once we get the link key */
	}
	BteHciConnectParams params = {
		s_packet_types,
		info->clock_offset,
		info->page_scan_rep_mode,
		true, /* Allow role switch */
	};
	BteL2CapConnectFlags flags = BTE_L2CAP_CONNECT_FLAG_AUTH;
	BteClient *client = bte_hci_get_client(hci);
	bte_l2cap_new_outgoing(client, &info->address, BTE_L2CAP_PSM_HID_CTRL,
						   &params, flags, hid_ctrl_connect_cb, device);
	return true;
}

static void read_remote_name_cb(BteHci *hci, const BteHciReadRemoteNameReply *r,
                                void *userdata)
{
	if (r->status == 0) {
		WPAD2_DEBUG("Got name %s for " BD_ADDR_FMT, r->name,
					BD_ADDR_DATA(&r->address));

		if (add_new_device(hci, &s_inquiry_responses->responses[s_inquiry_responses->num_names_queried], r->name)) {
			/* We only connect one Wiimote at a time: quit querying for names */
			return;
		}
	}

	s_inquiry_responses->num_names_queried++;
	query_name_next(hci);
}

static void query_name_next(BteHci *hci)
{
	if (s_inquiry_responses->num_names_queried >=
	    s_inquiry_responses->num_responses) {
		/* All names have been queried */
		return;
	}

	BteHciInquiryResponse *r =
		&s_inquiry_responses->responses[s_inquiry_responses->num_names_queried];
	bte_hci_read_remote_name(hci, &r->address, r->page_scan_rep_mode, r->clock_offset,
	                         NULL, read_remote_name_cb, NULL);
}

static void inquiry_cb(BteHci *hci, const BteHciInquiryReply *reply, void *)
{
	if (reply->status != 0) return;

	WPAD2_DEBUG("Results: %d", reply->num_responses);

	BteBdAddr known_devices[MAX_INQUIRY_RESPONSES];
	int num_known_devices = s_inquiry_responses->num_names_queried;
	for (int i = 0; i < num_known_devices; i++) {
		bd_address_copy(&known_devices[i],
		                &s_inquiry_responses->responses[i].address);
	}

	s_inquiry_responses->num_responses = 0;
	s_inquiry_responses->num_names_queried = 0;
	for (int i = 0; i < reply->num_responses; i++) {
		const BteHciInquiryResponse *r = &reply->responses[i];

		bool skip = false;
		for (int j = 0; j < num_known_devices; j++) {
			if (bd_address_is_equal(&r->address, &known_devices[j])) {
				/* Ignore this device, we've already queried its name and it's
				 * not an interesting device. */
				skip = true;
				break;
			}
		}

		const uint8_t *b = r->class_of_device.bytes;
		WPAD2_DEBUG(" - " BD_ADDR_FMT " COD %02x%02x%02x offs %d RSSI %d (skip = %d)",
		            BD_ADDR_DATA(&r->address),
		            b[2], b[1], b[0],
		            r->clock_offset, r->rssi, skip);
		if (skip) continue;

		/* New device, we'll query its name */
		if (s_inquiry_responses->num_responses >= MAX_INQUIRY_RESPONSES) break;
		BteHciInquiryResponse *resp =
			&s_inquiry_responses->responses[s_inquiry_responses->num_responses++];
		memcpy(resp, r, sizeof(*r));
	}

	query_name_next(hci);
}

static bool link_key_request_cb(BteHci *hci, const BteBdAddr *address,
                                void *userdata)
{
	WPAD2_DEBUG("Link key requested from " BD_ADDR_FMT, BD_ADDR_DATA(address));
	if (!wpad_device_is_pairing(address)) return false;

	const BteLinkKey *key = NULL;
	for (int i = 0; i < s_num_stored_keys; i++) {
		if (bd_address_is_equal(address, &s_stored_keys[i].address)) {
			key = &s_stored_keys[i].key;
			break;
		}
	}

	if (!key) {
		/* TODO: know if this is a registered device or a guest one */
		conf_pad_guest_device *guest = device_get_guest(address);
		if (guest) {
			key = (BteLinkKey *)guest->link_key;
		}
	}

	if (key) {
		bte_hci_link_key_req_reply(hci, address, key, NULL, NULL);
	} else {
		bte_hci_link_key_req_neg_reply(hci, address, NULL, NULL);
	}
	return true;
}

static bool pin_code_request_cb(BteHci *hci, const BteBdAddr *address,
                                void *userdata)
{
	WPAD2_DEBUG("PIN code requested from " BD_ADDR_FMT, BD_ADDR_DATA(address));
	if (!wpad_device_is_pairing(address)) return false;

	const BteBdAddr *pin = s_pair_mode == WPAD_PAIR_MODE_TEMPORARY ?
	                       address : &s_local_address;
	bte_hci_pin_code_req_reply(hci, address, (uint8_t *)pin, sizeof(*pin), NULL, NULL);
	return true;
}

static bool link_key_notification_cb(
	BteHci *hci, const BteHciLinkKeyNotificationData *data, void *userdata)
{
	WPAD2_DEBUG("Link key notification from " BD_ADDR_FMT ", type %d",
				BD_ADDR_DATA(&data->address), data->key_type);
	conf_pad_device *paired = device_get_paired(&data->address);

	if (paired) {
		/* Store the key on the BT controller */
		BteHciStoredLinkKey stored_key = {
			data->address,
			data->key,
		};
		bte_hci_write_stored_link_key(hci, 1, &stored_key, NULL, NULL);
	} else {
		conf_pad_guest_device *guest = device_get_guest(&data->address);
		if (!guest) return false;

		/* keys are stored backwards */
		const int key_size = sizeof(s_wpad_guests.guests[0].link_key);
		for (int i = 0; i < key_size; i++) {
			guest->link_key[i] = data->key.bytes[key_size - i];
		}

		CONF_SetPadGuestDevices(&s_wpad_guests);
	}
	return true;
}

static bool vendor_event_cb(BteHci *hci, BteBuffer *event_data, void *)
{
	if (event_data->size < 1) return false;

	const uint8_t *data = event_data->data + 2;
	bool held;
	if (data[0] == WPAD2_VENDOR_EVENT_PAIRING) {
		held = false;
	} else if (data[0] == WPAD2_VENDOR_EVENT_WIPE_PAIRED) {
		held = true;
	} else {
		return false;
	}

	WpadEvent event;
	event.channel = 0;
	event.type = WPAD2_EVENT_TYPE_HOST_SYNC_BTN;
	event.u.host_sync_button_held = held;
	s_worker_thread_event_cb(&event);
	return true;
}

typedef void (*HciNextFunction)(BteHci *hci);

static void generic_command_cb(BteHci *hci, const BteHciReply *reply, void *userdata)
{
	if (reply->status != 0) {
		WPAD2_ERROR("Command completed with status %d", reply->status);
		return;
	}

	HciNextFunction f = userdata;
	f(hci);
}

static void init_done(BteHci *hci)
{
	WpadEvent event;
	event.channel = 0;
	event.type = WPAD2_EVENT_TYPE_BT_INITIALIZED;
	s_worker_thread_event_cb(&event);
}

static void init_set_page_timeout(BteHci *hci)
{
	bte_hci_write_page_timeout(hci, 0x2000, generic_command_cb, init_done);
}

static void init_set_cod(BteHci *hci)
{
	BteClassOfDevice cod = {{0x04, 0x02, 0x40}};
	bte_hci_write_class_of_device(hci, &cod, generic_command_cb,
	                              init_set_page_timeout);
}

static void init_set_inquiry_scan_type(BteHci *hci)
{
	bte_hci_write_inquiry_scan_type(hci, BTE_HCI_INQUIRY_SCAN_TYPE_INTERLACED,
	                                generic_command_cb, init_set_cod);
}

static void init_set_page_scan_type(BteHci *hci)
{
	bte_hci_write_page_scan_type(hci, BTE_HCI_PAGE_SCAN_TYPE_INTERLACED,
	                             generic_command_cb, init_set_inquiry_scan_type);
}

static void init_set_inquiry_mode(BteHci *hci)
{
	bte_hci_write_inquiry_mode(hci, BTE_HCI_INQUIRY_MODE_RSSI,
	                           generic_command_cb, init_set_page_scan_type);
}

static void init_set_scan_enable(BteHci *hci)
{
	bte_hci_write_scan_enable(hci, BTE_HCI_SCAN_ENABLE_PAGE,
	                          generic_command_cb, init_set_inquiry_mode);
}

static void read_stored_link_keys_cb(
	BteHci *hci, const BteHciReadStoredLinkKeyReply *reply, void *)
{
	WPAD2_DEBUG("status %d, num keys %d, max %d", reply->status, reply->num_keys, reply->max_keys);
	if (reply->status == 0) {
		s_num_stored_keys = MIN(reply->num_keys, MAX_LINK_KEYS);
		for (int i = 0; i < s_num_stored_keys; i++) {
			s_stored_keys[i] = reply->stored_keys[i];
		}
	}
	init_set_scan_enable(hci);
}

static void init_read_stored_link_keys(BteHci *hci)
{
	bte_hci_read_stored_link_key(hci, NULL, read_stored_link_keys_cb, NULL);
}

static void init_set_local_name(BteHci *hci)
{
	bte_hci_write_local_name(hci, "Wii",
	                         generic_command_cb, init_read_stored_link_keys);
}

static void read_bd_addr_cb(BteHci *hci, const BteHciReadBdAddrReply *reply, void *userdata)
{
	s_local_address = reply->address;
	init_set_local_name(hci);
}

static bool connection_request_cb(BteL2capServer *l2cap_server,
                                  const BteBdAddr *address,
                                  const BteClassOfDevice *cod,
                                  void *userdata)
{
	WPAD2_DEBUG("from " BD_ADDR_FMT, BD_ADDR_DATA(address));


	/* If the device was already in the active list, allow it (and preserve its
	 * slot) */
	int active_slot = GetActiveSlot(address);
	if (active_slot >= 0) {
		WpadDevice *device = _wpad2_device_get(active_slot);
		if (device->hid_ctrl != NULL) {
			/* This can only occur if our structures are screwed up */
			WPAD2_WARNING("Conn request from " BD_ADDR_FMT " is already active",
						  BD_ADDR_DATA(address));
		}

		device_init(device, address);
		return true;
	}

	/* TODO: check device type */
	bool accepted = false;
	for (int i = 0; i < s_wpad_paired.num_registered; i++) {
		BteBdAddr stored = bd_address_from_conf(s_wpad_paired.registered[i].bdaddr);
		WPAD2_DEBUG("Stored %d: " BD_ADDR_FMT, i, BD_ADDR_DATA(&stored));
		if (bd_address_is_equal(address, &stored)) {
			accepted = true;
		}
	}

	/* Note that we don't check the guest device list here, because guest
	 * devices do not request a connection to the Wii, but listen in page mode.
	 */
	if (!accepted) return false;

	WpadDevice *device = device_allocate(address);
	if (!device) return false; /* No more slots */

	return true;
}

static bool decline_connection(BteL2capServer *l2cap_server,
                               const BteBdAddr *address,
                               const BteClassOfDevice *cod,
                               void *userdata)
{
	WPAD2_DEBUG("from " BD_ADDR_FMT, BD_ADDR_DATA(address));
	return false;
}

static void incoming_ctrl_connected_cb(
	BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata)
{
	WPAD2_DEBUG("l2cap != NULL %d", l2cap != NULL);
	if (!l2cap) {
		return;
	}

	device_hid_ctrl_connected(l2cap);
}

static void incoming_intr_connected_cb(
	BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata)
{
	WPAD2_DEBUG("l2cap != NULL %d", l2cap != NULL);
	if (!l2cap) {
		/* TODO: deallocate the WpadDevice */
		return;
	}
	device_hid_intr_connected(l2cap);
}

static void initialized_cb(BteHci *hci, bool success, void *)
{
	printf("Initialized, OK = %d\n", success);
	printf("ACL MTU=%d, max packets=%d\n",
	       bte_hci_get_acl_mtu(hci),
	       bte_hci_get_acl_max_packets(hci));
	BteHciFeatures features = bte_hci_get_supported_features(hci);
	s_packet_types = bte_hci_packet_types_from_features(features);
	bte_hci_on_link_key_request(hci, link_key_request_cb);
	bte_hci_on_pin_code_request(hci, pin_code_request_cb);
	bte_hci_on_link_key_notification(hci, link_key_notification_cb);
	bte_hci_on_vendor_event(hci, vendor_event_cb);
	bte_hci_read_bd_addr(hci, read_bd_addr_cb, NULL);

	s_l2cap_server_hid_ctrl = bte_l2cap_server_new(s_client,
	                                               BTE_L2CAP_PSM_HID_CTRL);
	s_l2cap_server_hid_intr = bte_l2cap_server_new(s_client,
	                                               BTE_L2CAP_PSM_HID_INTR);
	bte_l2cap_server_set_needs_auth(s_l2cap_server_hid_ctrl, true);
	bte_l2cap_server_set_role(s_l2cap_server_hid_ctrl, BTE_HCI_ROLE_MASTER);
	bte_l2cap_server_on_connected(s_l2cap_server_hid_ctrl,
	                              incoming_ctrl_connected_cb, NULL);
	bte_l2cap_server_on_connected(s_l2cap_server_hid_intr,
	                              incoming_intr_connected_cb, NULL);
	bte_l2cap_server_on_connection_request(s_l2cap_server_hid_ctrl,
	                                       connection_request_cb, NULL);
	/* Since HID clients are required to connect to the control PSM first, the
	 * ACL connection is always received on the BteL2capServer handling the
	 * control connection. */
	bte_l2cap_server_on_connection_request(s_l2cap_server_hid_intr,
	                                       decline_connection, NULL);
}

static void set_search_active(bool active)
{
	BteHci *hci = bte_hci_get(s_client);
	if (active) {
		/* wiiuse also clears the list of paired devices, but that doesn't seam
		 * entirely correct, since we are looking for guests here. */
		memset(&s_wpad_guests, 0, sizeof(s_wpad_guests));
		s_pair_mode = WPAD_PAIR_MODE_TEMPORARY;
		if (!s_inquiry_responses) {
			s_inquiry_responses = malloc(sizeof(*s_inquiry_responses));
			if (!s_inquiry_responses) return;
		}
		memset(s_inquiry_responses, 0, sizeof(*s_inquiry_responses));
		bte_hci_periodic_inquiry(hci, 4, 5, BTE_LAP_LIAC, 3, 0,
		                         NULL, inquiry_cb, NULL);
	} else {
		bte_hci_exit_periodic_inquiry(hci, NULL, NULL);
	}
}

static void device_disconnect(WpadDevice *device)
{
	if (device->hid_ctrl) {
		bte_l2cap_disconnect(device->hid_ctrl);
	}
	if (device->hid_intr) {
		bte_l2cap_disconnect(device->hid_intr);
	}

	disconnect_notify_and_free(device, BTE_HCI_CONN_TERMINATED_BY_LOCAL_HOST);
}

static void wipe_saved_controllers()
{
	WPAD2_DEBUG("");

	memset(&s_wpad_paired, 0, sizeof(s_wpad_paired));
	memset(&s_wpad_guests, 0, sizeof(s_wpad_guests));
	CONF_SetPadDevices(&s_wpad_paired);
	CONF_SetPadGuestDevices(&s_wpad_guests);
	CONF_SaveChanges();

	memset(&s_stored_keys, 0, sizeof(s_stored_keys));
	BteHci *hci = bte_hci_get(s_client);
	bte_hci_delete_stored_link_key(hci, NULL, NULL, NULL);
}

static void start_pairing()
{
	s_pair_mode = WPAD_PAIR_MODE_NORMAL;
	BteHci *hci = bte_hci_get(s_client);
	if (!s_inquiry_responses) {
		s_inquiry_responses = malloc(sizeof(*s_inquiry_responses));
		if (!s_inquiry_responses) return;
	}
	memset(s_inquiry_responses, 0, sizeof(*s_inquiry_responses));
	bte_hci_inquiry(hci, BTE_LAP_LIAC, 3, 0, NULL, inquiry_cb, NULL);
}

void process_client_message(uptr msg)
{
	uint32_t cmd = msg;
	uint32_t opcode = WORKER_CMD_OPCODE(cmd);
	uint32_t params = WORKER_CMD_PARAMS(cmd);
	int channel;
	switch (opcode) {
	case WORKER_CMD_SET_SEARCH:
		set_search_active(params);
		break;
	case WORKER_CMD_SET_FORMAT:
		channel = params >> 8;
		_wpad2_device_set_data_format(_wpad2_device_get(channel),
		                              params & 0xff);
		break;
	case WORKER_CMD_SET_MOTION_PLUS:
		channel = params >> 8;
		_wpad2_device_motion_plus_enable(_wpad2_device_get(channel),
		                                 params & 0xff);
		break;
	case WORKER_CMD_SET_RUMBLE:
		channel = params >> 8;
		_wpad2_device_set_rumble(_wpad2_device_get(channel), params & 0xff);
		break;
	case WORKER_CMD_SET_SPEAKER:
		channel = params >> 8;
		_wpad2_device_set_speaker(_wpad2_device_get(channel), params & 0xff);
		break;
	case WORKER_CMD_PLAY_SOUND:
		channel = params >> 8;
		WpadSoundInfo *sound_info = (void*)KMailboxRecv(&s_mailbox_in);
		_wpad2_speaker_play(_wpad2_device_get(channel), sound_info);
		break;
	case WORKER_CMD_DISCONNECT:
		channel = params >> 8;
		device_disconnect(_wpad2_device_get(channel));
		break;
	case WORKER_CMD_WIPE_CONTROLLERS:
		wipe_saved_controllers();
		break;
	case WORKER_CMD_START_PAIRING:
		start_pairing();
		break;
	}
}

static void device_calibration_cb(WpadDevice *device,
                                  const WpadDeviceCalibrationData *data)
{
	WpadEvent event;
	event.type = WPAD2_EVENT_TYPE_CALIBRATION;
	event.channel = _wpad2_device_get_slot(device);
	event.u.calibration_data = data;
	s_worker_thread_event_cb(&event);
}

static void device_exp_calibration_cb(WpadDevice *device,
                                      const WpadDeviceExpCalibrationData *data)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_EXP_CONNECTED;
	event.u.exp_connected.exp_type = device->exp_type;
	event.u.exp_connected.exp_subtype = device->exp_subtype;
	event.u.exp_connected.calibration_data = data;
	s_worker_thread_event_cb(&event);
}

static void device_exp_disconnected_cb(WpadDevice *device)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_EXP_DISCONNECTED;
	s_worker_thread_event_cb(&event);
}

static void device_ready_cb(WpadDevice *device)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_READY;
	s_worker_thread_event_cb(&event);
}

static void device_status_cb(WpadDevice *device)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_STATUS;
	event.u.status.battery_level = device->battery_level;
	event.u.status.battery_critical = device->battery_critical;
	event.u.status.speaker_enabled = device->speaker_enabled;
	s_worker_thread_event_cb(&event);
}

static void device_report_cb(WpadDevice *device, const uint8_t *data, uint8_t len)
{
	WpadEvent event;
	event.channel = _wpad2_device_get_slot(device);
	event.type = WPAD2_EVENT_TYPE_REPORT;
	event.u.report.data = data;
	event.u.report.len = len;
	s_worker_thread_event_cb(&event);
}

static void timer_handler_cb(WpadDevice *device, uint32_t period_us)
{
	WPAD2_DEBUG("device %p, period %d", device, period_us);
	/* Here we assume that we can use the same timer for all devices, that it
	 * that they all have the same period and start at the same time.
	 * This is fine as long as we don't support different audio formats for the
	 * speaker, because in that case we will have different periods. */
	if (period_us != 0) {
		s_timer_clients++;
	} else {
		s_timer_clients--;
	}

	if (s_timer_clients <= 0) {
		s_timer_period = 0;
	} else {
		if (s_timer_period == 0) {
			/* Here we assume that if a timer is already running, we don't have to
			 * restart it. */
			s_timer_last_trigger = get_time_us();
		}
		s_timer_period = period_us;
	}
}

static sptr worker_thread_func(void *)
{
	s_client = bte_client_new();
	BteHci *hci = bte_hci_get(s_client);
	bte_hci_on_initialized(hci, initialized_cb, NULL);

	_wpad2_device_set_calibration_cb(device_calibration_cb);
	_wpad2_device_set_exp_calibration_cb(device_exp_calibration_cb);
	_wpad2_device_set_exp_disconnected_cb(device_exp_disconnected_cb);
	_wpad2_device_set_ready_cb(device_ready_cb);
	_wpad2_device_set_status_cb(device_status_cb);
	_wpad2_device_set_report_cb(device_report_cb);
	_wpad2_device_set_timer_handler_cb(timer_handler_cb);

	CONF_GetPadDevices(&s_wpad_paired);
	CONF_GetPadGuestDevices(&s_wpad_guests);

	while (!s_worker_thread_done) {
		if (s_timer_period != 0) {
			uint64_t now = get_time_us();
			int32_t time_left = s_timer_period - (now - s_timer_last_trigger);
			if (time_left < 0) {
				/* In the (hopefully unlikely) case that we have been busy and
				 * are late on our timer, check for events without any sleep.
				 */
				bte_handle_events();
			} else {
				bte_wait_events(time_left);
			}
			now = get_time_us();
			if (now >= s_timer_last_trigger + s_timer_period) {
				s_timer_last_trigger = now;
				/* Trigger timer */
				_wpad2_device_timer_event(now);
			}
		} else {
			bte_wait_events(1000000);
		}

		if (is_initialized()) {
			uptr msg;
			while (KMailboxTryRecv(&s_mailbox_in, &msg)) {
				process_client_message(msg);
			}
		}
	}

	bte_client_unref(s_client);
	return 0;
}

void _wpad2_worker_start()
{
	KMailboxPrepare(&s_mailbox_in, s_mailbox_in_slots,
	                ARRAY_SIZE(s_mailbox_in_slots));

	s_worker_thread_stack = malloc(WORKER_THREAD_STACK_SIZE);
	/* TODO: figure out optimal priority */
	KThreadPrepare(&s_worker_thread, worker_thread_func, NULL,
	               s_worker_thread_stack + WORKER_THREAD_STACK_SIZE, KTHR_MAIN_PRIO);
	KThreadResume(&s_worker_thread);
}

void _wpad2_worker_on_event(WpadEventCb callback)
{
	s_worker_thread_event_cb = callback;
}

void _wpad2_worker_set_search_active(bool active)
{
	KMailboxTrySend(&s_mailbox_in, WORKER_CMD(SET_SEARCH, active));
}

void _wpad2_worker_set_format(uint8_t channel, uint8_t format)
{
	KMailboxTrySend(&s_mailbox_in,
	                WORKER_CMD(SET_FORMAT, (channel << 8) | format));
}

void _wpad2_worker_set_motion_plus(uint8_t channel, bool enable)
{
	KMailboxTrySend(&s_mailbox_in,
	                WORKER_CMD(SET_MOTION_PLUS, (channel << 8) | enable));
}

void _wpad2_worker_set_rumble(uint8_t channel, bool enable)
{
	KMailboxTrySend(&s_mailbox_in,
	                WORKER_CMD(SET_RUMBLE, (channel << 8) | enable));
}

void _wpad2_worker_set_speaker(uint8_t channel, bool enable)
{
	KMailboxTrySend(&s_mailbox_in,
	                WORKER_CMD(SET_SPEAKER, (channel << 8) | enable));
}

void _wpad2_worker_disconnect(uint8_t channel)
{
	KMailboxTrySend(&s_mailbox_in, WORKER_CMD(DISCONNECT, channel << 8));
}

void _wpad2_worker_stop()
{
	s_worker_thread_done = true;
	KThreadJoin(&s_worker_thread);
}

void _wpad2_worker_wipe_saved_controllers()
{
	KMailboxTrySend(&s_mailbox_in, WORKER_CMD_WIPE_CONTROLLERS);
}

void _wpad2_worker_start_pairing()
{
	KMailboxTrySend(&s_mailbox_in, WORKER_CMD_START_PAIRING);
}

void _wpad2_worker_play_sound(uint8_t channel, void *buffer, int len)
{
	WpadSoundInfo *info = malloc(sizeof(WpadSoundInfo) + len);
	if (!info) {
		WPAD2_WARNING("Could not allocate sound buffer (len = %d)", len);
		return;
	}

	info->len = len;
	info->offset = 0;
	memcpy(info->samples, buffer, len);

	KMailboxTrySend(&s_mailbox_in, WORKER_CMD(PLAY_SOUND, channel << 8));
	KMailboxTrySend(&s_mailbox_in, (uptr)info);
}
