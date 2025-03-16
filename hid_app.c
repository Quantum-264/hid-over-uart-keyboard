#include "bsp/board.h"
#include "tusb.h"
#include <unistd.h>

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const *report);

/// @brief Handle on USB HID device mount
/// @param dev_addr uint8_t (the HID address)
/// @param instance uint8_t (the HID instance)
/// @param desc_report uint8_t unused
/// @param desc_len uint16_t unused
/// @note tuh_hid_mount_cb is executed when a new device is mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
	if (itf_protocol == HID_ITF_PROTOCOL_NONE)
	{
		printf("Device with address %d, instance %d is not a keyboard or mouse.\r\n", dev_addr, instance);
		return;
	}
	const char *protocol_str[] = {"None", "Keyboard", "Mouse"};
	printf("Device with address %d, instance %d is a %s.\r\n", dev_addr, instance, protocol_str[itf_protocol]);

	// request to receive report
	// tuh_hid_report_received_cb() will be invoked when report is available
	if (!tuh_hid_receive_report(dev_addr, instance))
	{
		printf("Error: cannot request to receive report\r\n");
	}
}

/// @brief Handle data receipt from HID Mouse or Keyboard
/// @param dev_addr uint8_t (the HID address)
/// @param instance uint8_t (the HID instance)
/// @param report uint8_t pointer to the report
/// @param len uint16_t the length of the report
/// @note tuh_hid_report_received_cb is executed when data is received from the keyboard or mouse
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
	switch (tuh_hid_interface_protocol(dev_addr, instance))
	{
	case HID_ITF_PROTOCOL_KEYBOARD:
		process_kbd_report((hid_keyboard_report_t const *)report);
		break;
	case HID_ITF_PROTOCOL_MOUSE:
		process_mouse_report((hid_mouse_report_t const *)report);
		break;
	}

	// request to receive next report
	// tuh_hid_report_received_cb() will be invoked when report is available
	if (!tuh_hid_receive_report(dev_addr, instance))
	{
		printf("Error: cannot request to receive report\r\n");
	}
}

/**
 * Handle HID device unmount
 *
 * tuh_hid_umount_cb is executed when a device is unmounted.
 *
 * @param uint8_t dev_addr (the HID address),
 * @param uint8_t instance (the HID instance)
 */

/// @brief Handle HID device unmount
/// @param dev_addr uint8_t HID address
/// @param instance uint8_t HID instance
/// @note tuh_hid_umount_cb is executed when a device is unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	printf("Device with address %d, instance %d was unmounted.\r\n", dev_addr, instance);
}

/// @brief Check if key is held
/// @note check if keycode from existing report exists in a new report
/// @param report hid_keyboard_report_t (the current report)
/// @param keycode uint8_t keycode from existing report
/// @return boolean
static inline bool is_key_held(hid_keyboard_report_t const *report, uint8_t keycode)
{
	for (uint8_t i = 0; i < 6; i++)
	{
		if (report->keycode[i] == keycode)
		{
			return true;
		}
	}
	return false;
}

static uint8_t const keycode2ascii[128][2] = {HID_KEYCODE_TO_ASCII};

/// @brief Send the key event as HID-over-UART
/// @param pressed bool signifying key pressed or not
/// @param key uint8_t the HID keycode
/// @note this will send a key event message over UART\n
///
/// @details Message format:
/// | 15 - 13 	| start bits (Signifies start Key Event)
/// | 12 		| Press/Release bit
/// | 11 - 4	| HID Keycode (8-bit)
/// | 3 - 1		| Stop bits (Signifies end of Key event)
/// | 0			| Press/Release duplicate bit for verification
void send_key_event(bool pressed, uint8_t key)
{
	uint16_t packet = 0;

	packet |= (0b101 << 13);		   // Start bits (Key Event)
	packet |= (pressed ? 1 : 0) << 12; // Press/Release bit
	packet |= (key & 0xFF) << 4;	   // HID Keycode (8-bit)
	packet |= (0b011 << 1);			   // Stop bits (Key Event)
	packet |= (pressed ? 1 : 0);	   // Press/Release duplicate bit for verification

	uint8_t send_buffer[2] = {(packet >> 8) & 0xFF, packet & 0xFF};
	write(STDOUT_FILENO, send_buffer, 2);
}

/// @brief Send the modifier state as HID-over-UART
/// @param modifier_state the modifier bitfield
/// @note sends a bitfield of modifier keys states on change
///
/// @details Message format:
/// | 15 - 13 	| start bits (Signifies start Key Event)
/// | 12 		| unused
/// | 11 - 4	| HID Keycode (8-bit)
/// | 3 - 1		| Stop bits (Signifies end of Key event)
/// | 0			| unused (other than comparison against bit 12)
void send_modifier_event(uint8_t modifier_state)
{
	uint16_t packet = 0;

	packet |= (0b110 << 13);				// Start bits (Modifier Event)
	packet |= (modifier_state & 0xFF) << 4; // Modifier Byte (8-bit)
	packet |= (0b010 << 1);					// Stop bits (Modifier Event)

	uint8_t send_buffer[2] = {(packet >> 8) & 0xFF, packet & 0xFF};
	write(STDOUT_FILENO, send_buffer, 2);
}

/// @brief check for changes in the report against previous
/// @param prev_report the previous hid_keyboard_report_t
/// @param report the current hid_keyboard_report_t
/// @note Will send events if a change is detected
void detect_report_changes(const hid_keyboard_report_t *prev_report, const hid_keyboard_report_t *report)
{
	// Check if the modifier has changed
	if (prev_report->modifier != report->modifier)
	{
		// printf("Modifier changed: 0x%02X -> 0x%02X\n", prev_report->modifier, report->modifier);
		send_modifier_event(report->modifier);
	}

	// Check for newly released keys
	for (uint8_t i = 0; i < 6; i++)
	{
		if (prev_report->keycode[i] && !is_key_held(report, prev_report->keycode[i]))
		{
			// printf("Key Released: 0x%02X\n", prev_report->keycode[i]);
			send_key_event(false, prev_report->keycode[i]);
		}
	}

	// Check for newly pressed keys
	for (uint8_t i = 0; i < 6; i++)
	{
		if (report->keycode[i] && !is_key_held(prev_report, report->keycode[i]))
		{
			// printf("Key Pressed: 0x%02X\n", report->keycode[i]);
			send_key_event(true, report->keycode[i]);
		}
	}
}

/// @brief Process the keyboard HID report
/// @param report the hid_keyboard_report_t
static void process_kbd_report(hid_keyboard_report_t const *report)
{
	static hid_keyboard_report_t prev_report = {0, 0, {0}};

	// Compare the new report with the previous one
	detect_report_changes(&prev_report, report);

	prev_report = *report;
}

/// @brief Process the mouse HID report
/// @param report the hid_mouse_report_t
static void process_mouse_report(hid_mouse_report_t const *report)
{
	static hid_mouse_report_t prev_report = {0};

	// Mouse position.
	printf("Mouse: (%d %d %d)", report->x, report->y, report->wheel);

	// Button state.
	uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
	if (button_changed_mask & report->buttons)
	{
		printf(" %c%c%c",
			   report->buttons & MOUSE_BUTTON_LEFT ? 'L' : '-',
			   report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
			   report->buttons & MOUSE_BUTTON_RIGHT ? 'R' : '-');
	}

	printf("\n");
}
