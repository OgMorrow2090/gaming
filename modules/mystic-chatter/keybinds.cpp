/**
 * keybinds.cpp
 *
 * Keybind handler for Mystic Chatter addon.
 * Maps keybind identifiers to chat messages.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstring>

/**
 * ProcessKeybind - Called by Nexus when a registered keybind is triggered
 *
 * @param aIdentifier The keybind identifier string
 * @param aIsRelease  True if key was released (we only act on press)
 */
void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;

    if (strcmp(aIdentifier, CHAT_TY) == 0) {
        SendChatMessage("ty");
    } else if (strcmp(aIdentifier, CHAT_NP) == 0) {
        SendChatMessage("np");
    } else if (strcmp(aIdentifier, CHAT_HI) == 0) {
        SendChatMessage("Hi");
    } else if (strcmp(aIdentifier, CHAT_PARTY) == 0) {
        SendChatMessage("/party");
    } else if (strcmp(aIdentifier, CHAT_SAY) == 0) {
        SendChatMessage("/say");
    } else if (strcmp(aIdentifier, CHAT_MAP) == 0) {
        SendChatMessage("/map");
    } else if (strcmp(aIdentifier, CHAT_GUILD) == 0) {
        SendChatMessage("/guild");
    } else if (strcmp(aIdentifier, CHAT_SQUAD) == 0) {
        SendChatMessage("/squad");
    } else if (strcmp(aIdentifier, CHAT_QHEAL) == 0) {
        SendChatMessage("qheal");
    } else if (strcmp(aIdentifier, CHAT_AHEAL) == 0) {
        SendChatMessage("aheal");
    } else if (strcmp(aIdentifier, CHAT_PLUS) == 0) {
        SendChatMessage("+");
    } else if (strcmp(aIdentifier, CHAT_MINUS) == 0) {
        SendChatMessage("-");
    } else if (strcmp(aIdentifier, CHAT_YES) == 0) {
        SendChatMessage("Yes");
    } else if (strcmp(aIdentifier, CHAT_NO) == 0) {
        SendChatMessage("No");
    }
}
