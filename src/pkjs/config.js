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
      { type: "heading", defaultValue: "Data Slots (pick 4-6)" },
      {
        type: "select",
        messageKey: "Slot0",
        label: "Slot 1",
        defaultValue: "BB",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot1",
        label: "Slot 2",
        defaultValue: "STR",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot2",
        label: "Slot 3",
        defaultValue: "HR",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot3",
        label: "Slot 4",
        defaultValue: "STP",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot4",
        label: "Slot 5",
        defaultValue: "SLP",
        options: METRIC_OPTIONS,
      },
      {
        type: "select",
        messageKey: "Slot5",
        label: "Slot 6",
        defaultValue: "HRV",
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
