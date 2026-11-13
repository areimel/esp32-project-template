/** Generic template copy. Project identity from firmware/index.json overrides `project`. */
export const site = {
  project: {
    name: "ESP32 Starter",
    tagline: "A small, dependable starting point for ESP32 firmware.",
    description:
      "A PlatformIO and Arduino template for ESP32 boards with optional Bluetooth HID, WiFi, OTA and display modules, configured from a single YAML file and installable from the browser.",
  },
  /** Source link for forks: REPO_URL, else GitHub Actions' repository, else none. */
  repository:
    process.env.REPO_URL ||
    (process.env.GITHUB_REPOSITORY
      ? `https://github.com/${process.env.GITHUB_REPOSITORY}`
      : undefined),
  noRelease:
    "No release published yet. Build locally with pio run, or publish a release to enable browser install.",
  otaNote:
    "Release builds have OTA disabled. OTA is for local development builds, enabled by an ota_password in your own secrets.yaml.",
  features: [
    {
      title: "One config file",
      body: "Name, Bluetooth, WiFi, display and button gestures live in config.yaml and are validated at build time.",
    },
    {
      title: "Board profiles",
      body: "Pick an ESP32, ESP32-S3, ESP32-C3 or M5Stack Stick-S3 environment; board capabilities come from one table.",
    },
    {
      title: "Optional modules",
      body: "Bluetooth HID, WiFi with Improv provisioning, OTA and display are compiled in only when enabled.",
    },
    {
      title: "Safe browser flashing",
      body: "The installer writes bootloader, partitions and firmware only. Settings stored on the device are kept unless you choose to erase.",
    },
  ],
  steps: [
    "Connect your board over USB with a data cable.",
    "Choose your board below and press Install.",
    "Pick the serial port in the browser prompt and confirm.",
    "Wait for the flash to finish, then follow any WiFi or pairing prompts.",
  ],
  faq: [
    {
      q: "Which browsers work?",
      a: "Browsers with Web Serial: Chrome, Edge and Opera on desktop. Safari and Firefox cannot flash.",
    },
    {
      q: "The port does not appear.",
      a: "Use a USB data cable, close any serial monitor, and for some boards hold BOOT while plugging in to enter download mode.",
    },
    {
      q: "Does the released firmware support OTA updates?",
      a: "No. Release builds have OTA disabled. OTA is for local development builds, enabled by an ota_password in your own secrets.yaml.",
    },
    {
      q: "How do I recover a bad flash?",
      a: "Reflash from this page and choose to erase the device, or run pio run -t erase followed by pio run -t upload.",
    },
    {
      q: "Bluetooth will not pair after an update.",
      a: "Forget the device in your system Bluetooth settings, then pair again.",
    },
  ],
};
