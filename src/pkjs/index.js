var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

// ── Session state ─────────────────────────────────────────────────────────────
var session = {
  pending: false,
  token: localStorage.getItem('garmin_token') || '',
  tokenTime: parseInt(localStorage.getItem('garmin_token_time') || '0', 10)
};

// ── XHR helper ────────────────────────────────────────────────────────────────
function xhrRequest(method, url, body, headers, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    var text = '';
    try { text = xhr.responseText || ''; } catch(e) {}
    callback(null, xhr.status, text);
  };
  xhr.onerror = function() { callback('xhr_error', 0, ''); };
  xhr.open(method, url);
  if (headers) {
    Object.keys(headers).forEach(function(k) { xhr.setRequestHeader(k, headers[k]); });
  }
  xhr.send(body != null ? body : null);
}

// ── Garmin Connect login ──────────────────────────────────────────────────────
var DI_URL = 'https://diauth.garmin.com/di-oauth2-service/oauth/token';
var DI_GRANT = 'https://connectapi.garmin.com/di-oauth2-service/oauth/grant/service_ticket';
var DI_SERVICE = 'https://mobile.integration.garmin.com/gcm/ios';
var DI_CLIENT_IDS = [
  'GARMIN_CONNECT_MOBILE_IOS_DI',
  'GARMIN_CONNECT_MOBILE_ANDROID_DI',
  'GARMIN_CONNECT_MOBILE_ANDROID_DI_2024Q4',
  'GARMIN_CONNECT_MOBILE_ANDROID_DI_2025Q2'
];

function b64(str) {
  if (typeof btoa === 'function') return btoa(str);
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var out = '', i = 0;
  while (i < str.length) {
    var a = str.charCodeAt(i++), b = str.charCodeAt(i++), c = str.charCodeAt(i++);
    out += chars[a >> 2] + chars[((a & 3) << 4) | (b >> 4)] +
           (isNaN(b) ? '=' : chars[((b & 15) << 2) | (c >> 6)]) +
           (isNaN(c) ? '=' : chars[c & 63]);
  }
  return out;
}

function exchangeTicket(ticket, idx, callback) {
  if (idx >= DI_CLIENT_IDS.length) { console.log('DI exchange: all clients failed'); callback(false); return; }
  var clientId = DI_CLIENT_IDS[idx];
  var postBody = 'grant_type=' + encodeURIComponent(DI_GRANT) +
    '&client_id=' + encodeURIComponent(clientId) +
    '&service_ticket=' + encodeURIComponent(ticket) +
    '&service_url=' + encodeURIComponent(DI_SERVICE);

  xhrRequest('POST', DI_URL, postBody, {
    'Content-Type': 'application/x-www-form-urlencoded',
    'Authorization': 'Basic ' + b64(clientId + ':'),
    'Accept': 'application/json',
    'User-Agent': 'GCM-Android-5.23'
  }, function(err, status, body) {
    if (err || status !== 200) {
      console.log('DI ' + clientId + ' status=' + status);
      exchangeTicket(ticket, idx + 1, callback);
      return;
    }
    try {
      var data = JSON.parse(body);
      if (data.access_token) {
        session.token = data.access_token;
        session.tokenTime = Date.now();
        localStorage.setItem('garmin_token', data.access_token);
        localStorage.setItem('garmin_token_time', String(session.tokenTime));
        if (data.refresh_token) localStorage.setItem('garmin_refresh', data.refresh_token);
        localStorage.setItem('garmin_client_id', clientId);
        console.log('Garmin: token obtained (' + clientId + ')');
        fetchProfile(data.access_token);
        callback(true);
      } else {
        console.log('DI ' + clientId + ': no access_token');
        exchangeTicket(ticket, idx + 1, callback);
      }
    } catch(e) {
      exchangeTicket(ticket, idx + 1, callback);
    }
  });
}

function garminLogin(user, pass, callback) {
  var clientId = 'GCM_IOS_DARK';
  var service = 'https://mobile.integration.garmin.com/gcm/ios';
  var loginUrl = 'https://sso.garmin.com/mobile/api/login?clientId=' + clientId +
    '&locale=en-US&service=' + encodeURIComponent(service);
  var signinUrl = 'https://sso.garmin.com/mobile/sso/en/sign-in?clientId=' + clientId;

  xhrRequest('GET', signinUrl, null, {
    'User-Agent': 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15',
    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    'Accept-Language': 'en-US,en;q=0.9'
  }, function() {
    xhrRequest('POST', loginUrl, JSON.stringify({
      username: user,
      password: pass,
      rememberMe: false,
      captchaToken: ''
    }), {
      'Content-Type': 'application/json',
      'Accept': 'application/json',
      'User-Agent': 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15',
      'Accept-Language': 'en-US,en;q=0.9'
    }, function(err, status, body) {
    if (err || !body) { console.log('Garmin: login request failed'); callback(false); return; }
    console.log('Login status=' + status + ' body=' + body.substring(0, 100));
    try {
      var data = JSON.parse(body);
      var ticket = data.serviceTicketId;
      if (!ticket) { console.log('Garmin: no serviceTicketId'); callback(false); return; }
      exchangeTicket(ticket, 0, callback);
    } catch(e) {
      console.log('Garmin: login parse error');
      callback(false);
    }
    }); // POST
  }); // GET signin
}

function refreshToken(callback) {
  var refreshTok = localStorage.getItem('garmin_refresh') || '';
  var clientId = localStorage.getItem('garmin_client_id') || DI_CLIENT_IDS[0];
  if (!refreshTok) { callback(false); return; }

  var postBody = 'grant_type=refresh_token&client_id=' + encodeURIComponent(clientId) +
    '&refresh_token=' + encodeURIComponent(refreshTok);

  xhrRequest('POST', DI_URL, postBody, {
    'Content-Type': 'application/x-www-form-urlencoded',
    'Authorization': 'Basic ' + b64(clientId + ':'),
    'Accept': 'application/json',
    'User-Agent': 'GCM-Android-5.23'
  }, function(err, status, body) {
    if (err || status !== 200) { callback(false); return; }
    try {
      var data = JSON.parse(body);
      if (data.access_token) {
        session.token = data.access_token;
        session.tokenTime = Date.now();
        localStorage.setItem('garmin_token', data.access_token);
        localStorage.setItem('garmin_token_time', String(session.tokenTime));
        if (data.refresh_token) localStorage.setItem('garmin_refresh', data.refresh_token);
        console.log('Garmin: token refreshed');
        callback(true);
      } else { callback(false); }
    } catch(e) { callback(false); }
  });
}

function fetchProfile(token) {
  xhrRequest('GET', 'https://connectapi.garmin.com/userprofile-service/socialProfile', null, {
    'Authorization': 'Bearer ' + token,
    'Accept': 'application/json',
    'User-Agent': 'GCM-Android-5.23',
    'NK': 'NT'
  }, function(err, status, body) {
    if (!err && status === 200) {
      try {
        var d = JSON.parse(body);
        if (d.displayName) {
          localStorage.setItem('garmin_displayname', d.displayName);
          console.log('Garmin: displayName=' + d.displayName);
        }
      } catch(e) {}
    }
  });
}

// ── API endpoints ─────────────────────────────────────────────────────────────
var API_BASE = 'https://connectapi.garmin.com';
var API_PROXY = 'https://connect.garmin.com/modern/proxy';

function todayStr() {
  var d = new Date();
  return d.getFullYear() + '-' +
         ('0' + (d.getMonth() + 1)).slice(-2) + '-' +
         ('0' + d.getDate()).slice(-2);
}

function endpointUrl(metric) {
  var today = todayStr();
  var urls = {
    'HR':  API_BASE + '/wellness-service/wellness/dailyHeartRate?date=' + today,
    'SLP': API_BASE + '/wellness-service/wellness/dailySleepData?date=' + today,
    'BB':  API_BASE + '/wellness-service/wellness/bodyBattery/reports/daily?startDate=' + today + '&endDate=' + today,
    'STR': API_BASE + '/wellness-service/wellness/dailyStress/' + today,
    'HRV': API_BASE + '/hrv-service/hrv/' + today,
    'REC': API_BASE + '/metrics-service/metrics/trainingreadiness/' + today,
    'VO2': API_BASE + '/metrics-service/metrics/maxmet/daily/' + today + '/' + today,
    'FTP': API_BASE + '/biometric-service/biometric/powerToWeight/latest/' + today + '?sport=CYCLING',
    'MIN': API_BASE + '/wellness-service/wellness/daily/im/' + today,
    'TLD': API_BASE + '/metrics-service/metrics/trainingreadiness/' + today,
    'STP': API_BASE + '/wellness-service/wellness/dailySummaryChart/' + (localStorage.getItem('garmin_displayname') || 'me') + '?date=' + today,
    'SCH': API_BASE + '/wellness-service/wellness/dailySleepData?date=' + today + '&nonSleepBufferMinutes=60',
    'O2':  API_BASE + '/wellness-service/wellness/daily/spo2/' + today,
    'RSP': API_BASE + '/wellness-service/wellness/daily/respiration/' + today,
    'HEA': API_BASE + '/metrics-service/metrics/trainingstatus/aggregated/' + today,
    'ALT': API_BASE + '/metrics-service/metrics/trainingstatus/aggregated/' + today,
    'TST': API_BASE + '/metrics-service/metrics/trainingstatus/aggregated/' + today,
    'TRD': API_BASE + '/metrics-service/metrics/trainingreadiness/' + today
  };
  return urls[metric] || null;
}

function garminGet(url, callback) {
  xhrRequest('GET', url, null, {
    'Authorization': 'Bearer ' + session.token,
    'Accept': 'application/json',
    'User-Agent': 'GCM-Android-5.23',
    'X-App-Ver': '10861',
    'NK': 'NT',
    'X-requested-with': 'XMLHttpRequest'
  }, function(err, status, body) {
    var shortUrl = url.replace(/https?:\/\/[^\/]+/, '').substring(0, 55);
    if (err || status !== 200) {
      console.log('API FAIL ' + shortUrl + ' status=' + status);
      callback(null); return;
    }
    try {
      var data = JSON.parse(body);
      console.log('API OK ' + shortUrl);
      callback(data);
    } catch(e) {
      console.log('API JSON ERR ' + shortUrl + ' body=' + body.substring(0, 80));
      callback(null);
    }
  });
}

// ── Parse API responses ───────────────────────────────────────────────────────
function parseValue(metric, data) {
  if (!data) return null;
  var today = todayStr();
  switch (metric) {
    case 'BB':
      if (Array.isArray(data) && data.length > 0) {
        var bb = data[data.length - 1];
        if (bb.endBodyBatteryLevel !== undefined) return bb.endBodyBatteryLevel;
        if (bb.bodyBatteryValuesArray && bb.bodyBatteryValuesArray.length > 0) {
          var bba = bb.bodyBatteryValuesArray;
          return bba[bba.length - 1][1];
        }
        if (bb.startBodyBatteryLevel !== undefined) return bb.startBodyBatteryLevel;
        if (bb.charged !== undefined && bb.drained !== undefined) return bb.charged - bb.drained;
      }
      if (data.bodyBatteryFeedbackList && data.bodyBatteryFeedbackList.length > 0) {
        var last = data.bodyBatteryFeedbackList[data.bodyBatteryFeedbackList.length - 1];
        return last.bodyBatteryLevel !== undefined ? last.bodyBatteryLevel : null;
      }
      if (data.bodyBatteryValuesArray && data.bodyBatteryValuesArray.length > 0) {
        var a = data.bodyBatteryValuesArray; return a[a.length - 1][1];
      }
      return null;
    case 'STR':
      // New: {calendarDate, startTimestampGMT, endTimestampGMT, startTimestampLocal, endTimestampLocal, stressChartValueOffset, stressChartYAxisOrigin, stressValueDescriptorsDTOList, stressValuesArray}
      if (data.stressValuesArray) {
        var sa = data.stressValuesArray;
        for (var i = sa.length - 1; i >= 0; i--) { if (sa[i][1] >= 0) return sa[i][1]; }
      }
      if (data.avgStressLevel !== undefined && data.avgStressLevel >= 0) return data.avgStressLevel;
      return null;
    case 'HRV':
      if (data.hrvSummary) {
        var hs = data.hrvSummary;
        if (hs.lastNight !== undefined && hs.lastNight > 0) return hs.lastNight;
        if (hs.weeklyAvg !== undefined && hs.weeklyAvg > 0) return hs.weeklyAvg;
        if (hs.rmssd !== undefined) return hs.rmssd;
        if (hs.status) {
          var s = hs.status.toUpperCase();
          return s==='POOR'?'Poor': s==='BALANCED'?'Bal': s==='GOOD'?'Good': s.slice(0,4);
        }
      }
      if (Array.isArray(data) && data.length > 0) {
        var hd = data[0];
        if (hd.hrvSummary) {
          var hs2 = hd.hrvSummary;
          if (hs2.lastNight !== undefined && hs2.lastNight > 0) return hs2.lastNight;
          if (hs2.weeklyAvg !== undefined && hs2.weeklyAvg > 0) return hs2.weeklyAvg;
          if (hs2.rmssd !== undefined) return hs2.rmssd;
        }
      }
      if (data.lastNight) {
        var ln = data.lastNight;
        if (ln.rmssd5MinHigh !== undefined) return ln.rmssd5MinHigh;
        if (ln.rmssd !== undefined) return ln.rmssd;
      }
      return null;
    case 'REC':
      if (Array.isArray(data) && data.length > 0) {
        var rec = data[0];
        if (rec.recoveryTime !== undefined && rec.recoveryTime > 0) {
          var recHours = rec.recoveryTime / 60;
          if (rec.timestampLocal) {
            var measured = new Date(rec.timestampLocal.replace(' ', 'T'));
            var elapsed = (Date.now() - measured.getTime()) / 3600000;
            recHours = Math.max(0, recHours - elapsed);
          }
          return Math.round(recHours);
        }
        if (rec.recoveryTimeInMinutes !== undefined) return Math.round(rec.recoveryTimeInMinutes / 60);
        if (rec.score !== undefined) return rec.score;
        var lvl = rec.level || '';
        return lvl === 'HIGH' ? 85 : lvl === 'MODERATE' ? 55 : lvl === 'LOW' ? 25 : null;
      }
      if (data.recoveryTime !== undefined) return Math.round(data.recoveryTime / 60);
      return null;
    case 'VO2':
      if (Array.isArray(data) && data.length > 0) {
        var v = data[0];
        if (v.cycling && v.cycling.vo2MaxValue !== undefined) return Math.round(v.cycling.vo2MaxValue);
        if (v.generic && v.generic.vo2MaxValue !== undefined) return Math.round(v.generic.vo2MaxValue);
        if (v.vo2MaxValue !== undefined) return Math.round(v.vo2MaxValue);
      }
      if (data.vo2MaxValue !== undefined) return Math.round(data.vo2MaxValue);
      return null;
    case 'FTP':
      if (Array.isArray(data) && data.length > 0) {
        var f = data[0];
        if (f.functionalThresholdPower !== undefined) return f.functionalThresholdPower;
        if (f.ftpValue !== undefined) return f.ftpValue;
      }
      if (data.ftpValue !== undefined) return data.ftpValue;
      if (data.functionalThresholdPower !== undefined) return data.functionalThresholdPower;
      return null;
    case 'MIN':
      if (data.weeklyTotal !== undefined) return data.weeklyTotal;
      if (data.weeklyIntensityMinutes !== undefined) return data.weeklyIntensityMinutes;
      if (data.moderateIntensityMinutes !== undefined || data.vigorousIntensityMinutes !== undefined)
        return (data.moderateIntensityMinutes || 0) + (data.vigorousIntensityMinutes || 0) * 2;
      return null;
    case 'TLD':
      if (Array.isArray(data) && data.length > 0) {
        var tld = data[0];
        if (tld.acuteLoad !== undefined) return Math.round(tld.acuteLoad);
        if (tld.shortTermLoad !== undefined) return Math.round(tld.shortTermLoad);
      }
      if (data.acuteLoad !== undefined) return Math.round(data.acuteLoad);
      return null;
    case 'HR':
      if (data.restingHeartRate !== undefined && data.restingHeartRate > 0) return data.restingHeartRate;
      if (data.lastMeasurement !== undefined) return data.lastMeasurement;
      if (data.heartRateValues) {
        for (var j = data.heartRateValues.length - 1; j >= 0; j--) {
          if (data.heartRateValues[j] && data.heartRateValues[j][1] > 0) return data.heartRateValues[j][1];
        }
      }
      return null;
    case 'STP':
      if (Array.isArray(data)) {
        var totalStp = 0;
        for (var si2 = 0; si2 < data.length; si2++) {
          var se2 = data[si2];
          if (se2.steps !== undefined && se2.steps > 0) totalStp += se2.steps;
          else if (se2.totalSteps !== undefined && se2.totalSteps > totalStp) totalStp = se2.totalSteps;
        }
        if (totalStp > 0) return totalStp;
        var entry = data.filter(function(d) { return d.calendarDate === today; });
        if (entry.length > 0) return entry[0].totalSteps || entry[0].steps || 0;
        if (data.length > 0) return data[data.length-1].totalSteps || data[data.length-1].steps || 0;
      }
      if (data.totalSteps !== undefined) return data.totalSteps;
      return null;
    case 'SLP':
      if (data.dailySleepDTO && data.dailySleepDTO.sleepScores) {
        var ov = data.dailySleepDTO.sleepScores.overall;
        if (ov === null || ov === undefined) return null;
        return (typeof ov === 'object') ? (ov.value || null) : ov;
      }
      if (data.sleepScores && data.sleepScores.overall !== undefined) {
        var ov2 = data.sleepScores.overall;
        return (typeof ov2 === 'object') ? (ov2.value || null) : ov2;
      }
      return null;
    case 'SCH':
      if (data.dailySleepDTO) {
        var sc = data.dailySleepDTO.sleepScores;
        if (sc && sc.overall !== undefined) {
          var sv = (typeof sc.overall === 'object') ? sc.overall.value : sc.overall;
          if (sv !== null && sv !== undefined) return sv >= 80 ? 'Good' : sv >= 60 ? 'Fair' : 'Need';
        }
        if (data.dailySleepDTO.sleepCoachScore !== undefined) {
          var cs = data.dailySleepDTO.sleepCoachScore;
          return cs >= 80 ? 'Good' : cs >= 60 ? 'Fair' : 'Need';
        }
      }
      if (data.sleepCoachScore !== undefined)
        return data.sleepCoachScore >= 80 ? 'Good' : data.sleepCoachScore >= 60 ? 'Fair' : 'Need';
      return null;
    case 'O2':
      if (data.averageSpO2 !== undefined) return Math.round(data.averageSpO2);
      if (data.latestSpO2 !== undefined) return Math.round(data.latestSpO2);
      if (data.avgSleepSpO2 !== undefined) return Math.round(data.avgSleepSpO2);
      if (data.latestSpo2Value !== undefined) return data.latestSpo2Value;
      if (data.avgSpo2 !== undefined) return data.avgSpo2;
      return null;
    case 'RSP':
      if (data.avgWakingRespirationValue !== undefined) return Math.round(data.avgWakingRespirationValue);
      if (data.avgRespirationValue !== undefined) return Math.round(data.avgRespirationValue);
      if (data.avgSleepRespirationValue !== undefined) return Math.round(data.avgSleepRespirationValue);
      return null;
    case 'HEA':
      if (data.mostRecentVO2Max && data.mostRecentVO2Max.heatAltitudeAcclimation) {
        var haa = data.mostRecentVO2Max.heatAltitudeAcclimation;
        if (haa.heatAcclimationPercentage !== undefined) return Math.round(haa.heatAcclimationPercentage);
      }
      return null;
    case 'ALT':
      if (data.mostRecentVO2Max && data.mostRecentVO2Max.heatAltitudeAcclimation) {
        var haa2 = data.mostRecentVO2Max.heatAltitudeAcclimation;
        if (haa2.altitudeAcclimation !== undefined) return Math.round(haa2.altitudeAcclimation);
      }
      return null;
    case 'TST':
      if (data.mostRecentTrainingStatus && data.mostRecentTrainingStatus.latestTrainingStatusData) {
        var tsd = data.mostRecentTrainingStatus.latestTrainingStatusData;
        var devId = Object.keys(tsd)[0];
        if (devId) {
          var phrase = (tsd[devId].trainingStatusFeedbackPhrase || '').split('_')[0];
          var tsMap = {
            'PRODUCTIVE':'Prod', 'MAINTAINING':'Maint', 'PEAKING':'Peak',
            'RECOVERY':'Recov', 'UNPRODUCTIVE':'Unprd', 'OVERREACHING':'Over',
            'DETRAINING':'Detr', 'IMPROVING':'Imprv'
          };
          return tsMap[phrase] || (phrase ? phrase.slice(0, 6) : null);
        }
      }
      return null;
    case 'TRD':
      if (Array.isArray(data) && data.length > 0) {
        var trd = data[0];
        if (trd.score !== undefined) return trd.score;
      }
      return null;
  }
  return null;
}

var MSG_KEY = {
  'BB':'GARMIN_BB','STR':'GARMIN_STRESS','HRV':'GARMIN_HRV','REC':'GARMIN_RECOVERY',
  'VO2':'GARMIN_VO2MAX','FTP':'GARMIN_FTP','MIN':'GARMIN_INTMIN','TLD':'GARMIN_LOAD',
  'HR':'GARMIN_HR','STP':'GARMIN_STEPS','SLP':'GARMIN_SLEEP','SCH':'GARMIN_COACH','O2':'GARMIN_SPO2',
  'RSP':'GARMIN_RSP','HEA':'GARMIN_HEAT','ALT':'GARMIN_ALTACL','TST':'GARMIN_TSTATUS','TRD':'GARMIN_TREADY'
};

// ── Fetch and send ────────────────────────────────────────────────────────────
function getActiveMetrics() {
  var defaults = ['BB', 'STR', 'HR', 'STP', 'SLP', 'HRV'];
  var seen = {}, metrics = [];
  for (var i = 0; i < 6; i++) {
    var m = localStorage.getItem('garmin_slot' + i) || defaults[i] || 'NONE';
    if (m !== 'NONE' && !seen[m]) { seen[m] = true; metrics.push(m); }
  }
  return metrics;
}

function fetchAndSend() {
  var metrics = getActiveMetrics();
  if (metrics.length === 0) { Pebble.sendAppMessage({'GARMIN_STATUS': 1}); return; }

  Pebble.sendAppMessage({'GARMIN_STATUS': 2});
  var results = {};

  // Group metrics by URL to avoid duplicate requests
  var urlToMetrics = {};
  metrics.forEach(function(m) {
    var url = endpointUrl(m);
    if (!url) { results[m] = null; return; }
    if (!urlToMetrics[url]) urlToMetrics[url] = [];
    urlToMetrics[url].push(m);
  });

  var urls = Object.keys(urlToMetrics);
  if (urls.length === 0) { sendResults(results); return; }

  var pending = urls.length;
  urls.forEach(function(url) {
    garminGet(url, function(data) {
      urlToMetrics[url].forEach(function(m) {
        results[m] = parseValue(m, data);
      });
      if (--pending === 0) sendResults(results);
    });
  });
}

function sendResults(results) {
  var dict = {'GARMIN_STATUS': 1};
  Object.keys(results).forEach(function(m) {
    var key = MSG_KEY[m], val = results[m];
    if (key && val !== null && val !== undefined) dict[key] = val;
  });
  console.log('Sending: ' + JSON.stringify(dict));
  Pebble.sendAppMessage(dict,
    function() { console.log('Garmin data sent'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

function ensureLoggedIn(callback) {
  // Token valid for 55 min (expires at 60)
  if (session.token && (Date.now() - session.tokenTime) < 3300000) {
    callback(true); return;
  }
  if (session.pending) { console.log('Garmin: login in progress'); return; }

  var user = localStorage.getItem('garmin_user') || '';
  var pass = localStorage.getItem('garmin_pass') || '';
  if (!user || !pass) {
    console.log('Garmin: no credentials');
    Pebble.sendAppMessage({'GARMIN_STATUS': 0});
    return;
  }

  session.pending = true;

  // Try refresh token first
  var refreshTok = localStorage.getItem('garmin_refresh') || '';
  if (refreshTok) {
    refreshToken(function(ok) {
      if (ok) { session.pending = false; callback(true); }
      else {
        garminLogin(user, pass, function(ok2) {
          session.pending = false;
          if (!ok2) Pebble.sendAppMessage({'GARMIN_STATUS': 0});
          callback(ok2);
        });
      }
    });
  } else {
    garminLogin(user, pass, function(ok) {
      session.pending = false;
      if (!ok) Pebble.sendAppMessage({'GARMIN_STATUS': 0});
      callback(ok);
    });
  }
}

// ── Pebble events ─────────────────────────────────────────────────────────────
Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  var named = clay.getSettings(e.response, false);
  var prevUser = localStorage.getItem('garmin_user') || '';
  var prevPass = localStorage.getItem('garmin_pass') || '';
  var newUser = named.GarminUser ? (named.GarminUser.value || '') : prevUser;
  var newPass = named.GarminPass ? (named.GarminPass.value || '') : prevPass;
  if (named.GarminUser) localStorage.setItem('garmin_user', newUser);
  if (named.GarminPass) localStorage.setItem('garmin_pass', newPass);
  for (var i = 0; i < 6; i++) {
    var sk = 'Slot' + i;
    if (named[sk]) localStorage.setItem('garmin_slot' + i, named[sk].value || 'NONE');
  }
  // Force re-login only if credentials changed
  if (newUser !== prevUser || newPass !== prevPass) {
    session.token = '';
    session.tokenTime = 0;
    session.pending = false;
    localStorage.removeItem('garmin_token');
    localStorage.removeItem('garmin_refresh');
  }

  var dict = clay.getSettings(e.response);
  Pebble.sendAppMessage(dict,
    function() { console.log('Sent config data to Pebble'); },
    function(err) { console.log('Failed to send config: ' + JSON.stringify(err)); }
  );
  ensureLoggedIn(function(ok) { if (ok) fetchAndSend(); });
});

Pebble.addEventListener('ready', function() {
  ensureLoggedIn(function(ok) { if (ok) fetchAndSend(); });
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload['REQUEST_GARMIN']) {
    ensureLoggedIn(function(ok) { if (ok) fetchAndSend(); });
  }
});
