# Changelog

Each release needs a section here headed with its version, exactly as in `firmware/version.txt`.
The text under it (keep it short and plain) is shown on the settings website when the update
is offered.

## 0.1.6
- The settings page now looks and works like PocketOS: the same glass tiles and lists, PocketOS’s own lettering, and a live picture of your PocketOS at the top
- Open a section from its tile, then tap ‹ or swipe right to go back, just like on PocketOS. Your phone’s back gesture works too

## 0.1.5
- Replies to voice requests are now spoken out loud as well as shown. Turn it off, or set the volume, under Smart home → Voice on the settings page
- Lights that support it can be set warmer or cooler, or to a colour: use the buttons beside the power button on a light’s screen
- Homes set to Fahrenheit show temperatures on the right scale
- More time zones to choose from

## 0.1.4
- If your Wi-Fi password changes, PocketOS says so and the Boot button takes you straight to Wi-Fi setup
- Change Wi-Fi network and Reset PocketOS are now on PocketOS itself, under More → This PocketOS, confirmed with the Boot button
- Updates must be signed by PocketOS: firmware from anywhere else is refused
- New versions can be released gradually, and paused if a problem turns up
- PocketOS joins WPA2 Wi-Fi or better, and shares no network or home details with anyone who hasn't paired

## 0.1.3
- Works with larger homes: up to 256 devices, with lights, heating and locks always loaded before sensors
- Sensors now show room temperature and humidity only, so power and signal readings don't crowd your lists
- “Sending…” clears as soon as Home Assistant confirms a change, and a refused change shows Home Assistant’s reason once
- Rooms update by themselves when you move a device to another room in Home Assistant
- No more brief “Can’t reach home” while PocketOS starts up or Home Assistant restarts
- Lists say when they're waiting for Home Assistant, instead of looking empty

## 0.1.2
- Press and hold any device on PocketOS, or tap ··· on its screen, to add it to favourites, change its place in Favourites, or hide it
- Hidden devices are listed under More, so you can bring them back at any time
- Rename devices on the settings page. PocketOS shows your name; Home Assistant keeps its own
- Put your favourites in the order you want, on PocketOS or on the settings page

## 0.1.1
- PocketOS now starts with the PocketOS logo
- Hold the Power button for one second to switch on, so it won’t turn on by accident in a pocket. Hold it for six seconds to switch off
- Clearer wording for the Power and Boot buttons on PocketOS and its settings page
- Update options are now called Notify, Install overnight and Off

## 0.1.0
Initial release.
- Companion that reflects the state of your home
- Control of lights, heating, media, locks, blinds, plugs and scenes through Home Assistant
- Settings page, push-to-talk voice, battery insight and automatic updates
