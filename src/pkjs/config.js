var TOP_SLOT_OPTIONS = [
  { label: "Date",            value: "DATE" },
  { label: "Battery",         value: "BAT"  },
  { label: "Heart Rate",      value: "HR"   },
  { label: "Steps",           value: "STP"  },
  { label: "Sleep",           value: "SLP"  },
  { label: "Sunrise/Sunset",  value: "SRS"  },
  { label: "Weather",         value: "WTH"  },
];

var METRIC_OPTIONS = [
  { label: "--- None ---",       value: "NONE" },
  // Daily Wellness
  { label: "Body Battery",       value: "BB"   },
  { label: "Stress",             value: "STR"  },
  { label: "Heart Rate",         value: "HR"   },
  { label: "Steps",              value: "STP"  },
  { label: "Blood Oxygen",       value: "O2"   },
  { label: "Respiration",        value: "RSP"  },
  // Sleep
  { label: "Sleep Score",        value: "SLP"  },
  { label: "Sleep Coach",        value: "SCH"  },
  // Recovery
  { label: "HRV",                value: "HRV"  },
  { label: "Recovery Time",      value: "REC"  },
  { label: "Training Readiness", value: "TRD"  },
  // Training / Performance
  { label: "VO2Max",             value: "VO2"  },
  { label: "Cycling FTP",        value: "FTP"  },
  { label: "Weekly Int. Min",    value: "MIN"  },
  { label: "Training Load",      value: "TLD"  },
  { label: "Training Status",    value: "TST"  },
  // Acclimation
  { label: "Heat Acclimation",   value: "HEA"  },
  { label: "Alt. Acclimation",   value: "ALT"  },
];

module.exports = [
  { type: "heading", defaultValue: "Garmin Fitness Face" },
  {
    type: "text",
    defaultValue:
      "Warning: credentials are stored in plain text. Disable 2FA on your Garmin account before use.",
  },
  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Top Section (Pebble)" },
      {
        type: "select",
        messageKey: "TopSlot0",
        label: "Top Left",
        defaultValue: "DATE",
        options: TOP_SLOT_OPTIONS,
      },
      {
        type: "select",
        messageKey: "TopSlot1",
        label: "Top Center",
        defaultValue: "BAT",
        options: TOP_SLOT_OPTIONS,
      },
      {
        type: "select",
        messageKey: "TopSlot2",
        label: "Top Right",
        defaultValue: "HR",
        options: TOP_SLOT_OPTIONS,
      },
    ],
  },
  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Garmin Connect Login" },
      {
        type: "input",
        messageKey: "GarminUser",
        label: "Username / Email",
        defaultValue: "",
      },
      {
        type: "input",
        messageKey: "GarminPass",
        label: "Password",
        defaultValue: "",
        attributes: { type: "password" },
      },
    ],
  },
  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Bottom Section (Garmin)" },
      {
        type: "select",
        messageKey: "Slot0",
        label: "Bottom 1st",
        defaultValue: "BB",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot1",
        label: "Bottom 2nd",
        defaultValue: "STR",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot2",
        label: "Bottom 3rd",
        defaultValue: "HRV",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot3",
        label: "Bottom 4th",
        defaultValue: "REC",
        options: METRIC_OPTIONS,
      },
    ],
  },
  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Update Frequency" },
      {
        type: "select",
        messageKey: "UpdateInterval",
        label: "Refresh Garmin Data",
        defaultValue: "30",
        options: [
          { label: "Every 10 min", value: "10" },
          { label: "Every 20 min", value: "20" },
          { label: "Every 30 min", value: "30" },
          { label: "Every 50 min", value: "50" },
        ],
      },
    ],
  },
  { type: "submit", defaultValue: "Save Settings" },
];
