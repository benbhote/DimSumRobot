#include "Config.h"
#include "WebServer.h"

// Creating SSE events flow SSE on "/events" URL
AsyncEventSource events("/events");

// Creating WebSocket on "/ws" URL to fludify steering
AsyncWebSocket ws("/ws");

// Messages administrator recieved by the websocket for the motors
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      String msg = (char*)data;
      
      timestampJoystick = millis(); // Moving's watchdog

      if (msg.indexOf("\"state\":\"stop\"") != -1) {
        consigneThrottle = 0;
        consigneSteering = 0;
        getMoveOrder = false;
      } else if (msg.indexOf("throttle") != -1 && msg.indexOf("steering") != -1) {
        // Quick basic extract of JSON data recieved
        int tPos = msg.indexOf("throttle");
        int sPos = msg.indexOf("steering");
        if (tPos != -1 && sPos != -1) {
          int tColon = msg.indexOf(':', tPos);
          int tEnd = msg.indexOf(',', tColon);
          if (tEnd == -1) tEnd = msg.indexOf('}', tColon);
          int throttleVal = msg.substring(tColon + 1, tEnd).toInt();

          int sColon = msg.indexOf(':', sPos);
          int sEnd = msg.indexOf('}', sColon);
          int steeringVal = msg.substring(sColon + 1, sEnd).toInt();

          consigneThrottle = throttleVal;

          // Applying coeff and steering limitations
          float rawSteering = (float)steeringVal;
          float adjustedSteering = rawSteering * 0.5f;
          float maxSteeringLimit = 48.0f;
          if (adjustedSteering > maxSteeringLimit)  adjustedSteering = maxSteeringLimit;
          if (adjustedSteering < -maxSteeringLimit) adjustedSteering = -maxSteeringLimit;
          consigneSteering = (int)(adjustedSteering);

          // Assess moving request
          bool moveRequest = (consigneThrottle != 0 || consigneSteering != 0);

          if (moveRequest && !getMoveOrder) {
            getMoveOrder = true;
            if (radarTaskHandle != NULL) {
              xTaskNotifyGive(radarTaskHandle); // Instant wake up of the radar
            }
          } 
          else if (!moveRequest) {
            getMoveOrder = false;
          }
        }
      }
    }
  }
}

// Return true if system is busy (with 429 code), false otherwise
bool isSystemBusy(AsyncWebServerRequest *request) {
  if (isRequesting) {
    request->send(429, "text/plain", "System busy, please wait");
    return true;
  }
  return false;
}

// --- Asynchrone web server configuration ---
void configWebServer(AsyncWebServer &server) {
  // 1. SSE events administratior and WebSocket declaration
  server.addHandler(&events);
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // 2. Endpoint API for diagnosis (only return JSON)
  server.on("/diag", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
      json += "\"camera\":" + String(statusCamera ? "true" : "false") + ",";
      json += "\"sd\":" + String(statusSD ? "true" : "false") + ",";
      json += "\"mpu\":" + String(statusMPU ? "true" : "false") + ",";
      json += "\"sht40\":" + String(statusSHT40 ? "true" : "false") + ",";
      json += "\"radar\":" + String(statusRadar ? "true" : "false")+ ",";
      json += "\"modeEquilibrium\":" + String(modeEquilibrium ? "true" : "false");
      json += "}";
    request->send(200, "application/json", json);
  });

  // 3. Main Endpoint (Single Page Application interface)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", R"rawliteral(
      <!DOCTYPE HTML>
      <html>
        <head>
          <meta charset="utf-8">
          <title>RobAstroTop Control</title>
          <style>
            body { font-family: Arial; text-align: center; margin: 0; padding: 15px; background: #121212; color: #e0e0e0; }
            .view { display: none; }
            .active { display: block; }
            img { width: 100%; max-width: 480px; height: auto; border: 2px solid #333; border-radius: 8px; }
            .btn { padding: 10px 20px; font-size: 15px; margin: 5px; cursor: pointer; background: #007ACC; color: white; border: none; border-radius: 5px; }
            .btn:active { background: #005999; }
            .btn-danger { background: #d9534f; }
            .btn:disabled { background-color: #555 !important; color: #999 !important; cursor: not-allowed; opacity: 0.6; }
            .nav-bar { margin-bottom: 20px; border-bottom: 1px solid #333; padding-bottom: 10px; }
            .sensor-box { background: #1e1e1e; margin: 10px auto; padding: 8px; max-width: 480px; border-radius: 5px; font-size: 14px; border: 1px solid #333; }
            .error-text { color: #ff4d4d; font-weight: bold; }
            .file-row { display: flex; justify-content: space-between; align-items: center; background: #1e1e1e; margin: 5px auto; padding: 8px; max-width: 400px; border-radius: 4px; }
            #joystick-zone { width: 300px; height: 300px; background: #222; border: 2px solid #555; border-radius: 50%; margin: 20px auto; position: relative; touch-action: none; }
            #joystick-handle { width: 50px; height: 50px; background: #007ACC; border-radius: 50%; position: absolute; top: 125px; left: 125px; }
            .warning-box { background: #5c3a21; color: #ffcc00; padding: 10px; border-radius: 5px; margin: 10px auto; max-width: 480px; display: none; }
          </style>
        </head>

        <body>
          <div class="nav-bar">
            <button id="nav-diag" class="btn" onclick="switchView('diagnostic'); fetch('/sd-stats');">Diagnostic</button>
            <button id="nav-obs" class="btn" onclick="switchView('observation'); fetch('/observation');">Observation</button>
            <button id="nav-expl" class="btn" onclick="switchView('exploration'); fetch('/exploration');">Exploration</button>
            <button id="nav-gallery" class="btn" onclick="switchView('files'); fetch('/gallery');">Gallery SD</button>
          </div>

          <!-- Global sensors box -->
          <div id="global-sensor-box" class="sensor-box" style="margin: 10px auto; max-width: 480px; text-align: center;">
            Temperature : <span id="temp">--</span> °C | Humidity : <span id="hum">--</span> %
          </div>

          <!-- VUE 0 : DIAGNOSTIC (default) -->
          <div id="view-diagnostic" class="view active">
            <h2>System state (Diagnostic)</h2>
            <div class="sensor-box" style="text-align: left; max-width: 400px;">
              <p>Camera : <span id="diag-cam">--</span></p>
              <p>SD card : <span id="diag-sd">--</span></p>
              <p>SD space used : <span id="sd-percent">--</span> %</p>
              <p>MPU6050 : <span id="diag-mpu">--</span></p>
              <p>SHT40 : <span id="diag-sht">--</span></p>
              <p>Radar : <span id="diag-radar">--</span></p>
            </div>
            <button class="btn" onclick="verifyDiagAndSafety()">Update diagnostic</button>
          </div>

          <!-- Global camera container -->
          <div id="global-video-container" style="margin: 15px auto; max-width: 480px; text-align: center;">
            <img id="videoStream" src="" style="width: 100%; border-radius: 8px; border: 2px solid #333;">
          </div>

          <!-- VUE 1 : OBSERVATION -->
          <div id="view-observation" class="view">
            <h2>Observation mode</h2>
            <div style="margin-top: 15px;">
              <button class="btn" onclick="takeSnapshot()">Snapshot</button>
              <button id="btn-hd" class="btn" style="background: #28a745;" onclick="triggerCapture('hd')">Photo HD</button>
              <button id="btn-astro" class="btn" style="background: #6f42c1;" onclick="triggerCapture('astro')">Photo Astro</button>
            </div>
          </div>

          <!-- VUE 2 : EXPLORATION -->
          <div id="view-exploration" class="view">
            <h2>Exploration mode</h2>
            <div id="radar-warning" class="warning-box">Caution : radar malfunctioning !</div>
            <div style="margin-top: 15px;">
              <button class="btn" onclick="toggleStream()">Start / Stop Stream</button>
              <button id="btn-motors-mode" class="btn" style="background: #e67e22;" onclick="toggleMotorsMode()">Stabilisation : OFF</button>
            </div>

            <p>Use joystick to move robot</p>
            <div id="joystick-zone">
              <div id="joystick-handle"></div>
            </div>

            <!-- Sensors test box -->
            <div class="sensor-box" style="margin: 15px auto; text-align: left; max-width: 480px;">
              <h3 style="text-align: center; margin-top: 5px; color: #007ACC;">Test sensors</h3>
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                <span>Radar : <strong id="radar-val">--</strong> cm</span>
                <button class="btn" style="padding: 5px 10px; font-size: 13px; margin: 0;" onclick="testRadar()">Radar distance</button>
              </div>
              <div style="display: flex; justify-content: space-between; align-items: center;">
                <span>Pitch : <strong id="mpu-pitch">--</strong> °</span>
                <span>Roll : <strong id="mpu-roll">--</strong> °</span>
                <span>Gravity : <strong id="mpu-gravity">--</strong> g</span>
                <button class="btn" style="padding: 5px 10px; font-size: 13px; margin: 0;" onclick="testMPU()">MPU data</button>
              </div>
            </div>
          </div>

          <!-- VUE 3 : GALLERY SD -->
          <div id="view-files" class="view">
            <h2>Gallery</h2>
            <div id="file-list">Files loading...</div>
          </div>

          <script>
            let streamActive = false;
            let ws = null;

            // WebSocket initialization for the joystick
            function initWebSocket() {
              ws = new WebSocket('ws://' + window.location.host + '/ws');
              ws.onopen = function() { console.log("[WS] Connected to the server"); };
              ws.onclose = function() {
                console.warn("[WS] Disconnected. Trying to reconnect...");
                setTimeout(initWebSocket, 2000);
              };
              ws.onerror = function(err) { console.error("[WS] Error :", err); };
            }

            // Auto launch web page loads
            window.onload = function() {
              verifyDiagAndSafety();
              initWebSocket();

              // --- Real time connection to events of ESP32 (SSE) ---
              if (!!window.EventSource) {
                const source = new EventSource('/events');

                source.addEventListener('system_state', function(e) {
                  if (e.data === "mpu_ko") {
                    console.warn("[UI] Critical alert : MPU malfuctioning !");
                    
                    let btnMotorsMode = document.getElementById('btn-motors-mode');
                    if (btnMotorsMode) {
                      btnMotorsMode.innerText = "Safety : Motors only";
                      btnMotorsMode.disabled = true;
                      btnMotorsMode.style.background = "#555";
                    }
                  }
                }, false);
              }
            };

            function testRadar() {
              let el = document.getElementById('radar-val');
              el.innerText = "...";
              fetch('/test-radar')
              .then(res => res.json())
              .then(data => {
                if (data.distance === -1) {
                  el.innerText = "Out of reach";
                } else {
                  el.innerText = data.distance;
                }
                verifyDiagAndSafety(); // Update radar status in case of change
              })
              .catch(err => {
                el.innerText = "Error";
                console.error(err);
              });
            }

            function testMPU() {
              let pitchEl = document.getElementById('mpu-pitch');
              let rollEl = document.getElementById('mpu-roll');
              let gravEl = document.getElementById('mpu-gravity');
              
              pitchEl.innerText = "...";
              rollEl.innerText = "...";
              gravEl.innerText = "...";
              
              fetch('/test-mpu')
              .then(res => res.json())
              .then(data => {
                pitchEl.innerText = data.pitch.toFixed(1);
                rollEl.innerText = data.roll.toFixed(1);
                gravEl.innerText = data.gravity.toFixed(2);
              })
              .catch(err => {
                pitchEl.innerText = "Err";
                rollEl.innerText = "Err";
                gravEl.innerText = "Err";
                console.error(err);
              });
            }

            function verifyDiagAndSafety() {
              fetch('/diag')
              .then(res => res.json())
              .then(data => {
				// 1. Display of status on Diagnostic page
                document.getElementById('diag-cam').innerText = data.camera ? "OK (PSRAM OK)" : "KO (No PSRAM)";
                document.getElementById('diag-sd').innerText = data.sd ? "OK" : "KO";
                document.getElementById('diag-mpu').innerText = data.mpu ? "OK" : "KO";
                document.getElementById('diag-sht').innerText = data.sht40 ? "OK" : "KO";
                document.getElementById('diag-radar').innerText = data.radar ? "OK" : "KO";

				// 2. Access management						
                let btnObs = document.getElementById('nav-obs');
                let btnExp = document.getElementById('nav-expl');
                let btnGal = document.getElementById('nav-gallery');
                let sensorBox = document.getElementById('global-sensor-box');
                let sensorTemp = document.getElementById('temp');
                let sensorHum = document.getElementById('hum');

				// Observation needs Camera & SD card											 
                btnObs.disabled = (!data.camera || !data.sd);
				// Gallery needs SD	card						  
                btnGal.disabled = (!data.sd);

				// --- Dynamic control of stabilisation button / motors mode ---																	  
                let btnMotorsMode = document.getElementById('btn-motors-mode');
                if (btnMotorsMode) {
                  if (!data.mpu) {
					// If MPU KO : locking button in Safety : motors only																	
                    btnMotorsMode.innerText = "Safety : Motors only";
                    btnMotorsMode.disabled = true;
                    btnMotorsMode.style.background = "#555";
                  } else {
					// If MPU OK : button usable and show real state
                    btnMotorsMode.disabled = false;
                    if (data.modeEquilibrium) {
                      btnMotorsMode.innerText = "Stabilisation : ON";
                      btnMotorsMode.style.background = "#27ae60";
                    } else {
                      btnMotorsMode.innerText = "Stabilisation : OFF";
                      btnMotorsMode.style.background = "#e67e22";
                    }
                  }
                }

				// If SHT40 KO -> Hide value box														 
                if (data.sht40) {																										 
                  sensorTemp.classList.remove('error-text');
                  sensorHum.classList.remove('error-text');
                } else {														  
                  sensorTemp.innerText = "ND";
                  sensorHum.innerText = "ND";
                  sensorTemp.classList.add('error-text');
                  sensorHum.classList.add('error-text');
                }

				// If Radar KO or no value -> warning on Exploration page									
                let radarWarning = document.getElementById('radar-warning');
                if (radarWarning) {
                  radarWarning.style.display = data.radar ? 'none' : 'block';
                }
              });

              fetch('/sd-stats')
              .then(res => res.json())
              .then(data => {
                let sdPercentElement = document.getElementById('sd-percent');
                if(sdPercentElement) {
                  sdPercentElement.innerText = data.percent;
                }
              });
            }

            function takeSnapshot() {
              let activeView = document.querySelector('.view.active').id;
              if (activeView !== 'view-observation') return;
              let img = document.getElementById('videoStream');
              img.src = "/snapshot?t=" + new Date().getTime();
            }

            function toggleStream() {
              let activeView = document.querySelector('.view.active').id;
              if (activeView !== 'view-exploration') return;
              let img = document.getElementById('videoStream');
              if (streamActive) {
                streamActive = false;
                img.src = ""; 
              } else {
                streamActive = true;
                loadNextFrame();
              }
            }

            function loadNextFrame() {
              if (!streamActive) return;

              let img = document.getElementById('videoStream');
			  // Temporary frame to load framework
              let tempImg = new Image();
              
              tempImg.onload = function() {
                if (!streamActive) return;
				// Display of frame					   
                img.src = tempImg.src;
				// New frame				 
                setTimeout(loadNextFrame, 10);
              };

              tempImg.onerror = function() {
                if (!streamActive) return;
				// If failed, break before requesting new frame
                setTimeout(loadNextFrame, 200);
              };														   
              tempImg.src = "/snapshot?t=" + new Date().getTime();
            }

            function toggleMotorsMode() {
              fetch('/toggleMotors')
              .then(res => res.text())
              .then(status => {
                let btn = document.getElementById('btn-motors-mode');
                if (status === "ON") {
                  btn.innerText = "Stabilisation : ON";
                  btn.style.background = "#27ae60";
                } else {
                  btn.innerText = "Stabilisation : OFF";
                  btn.style.background = "#e67e22";
                }
              })
              .catch(err => {
                console.error("Error when switching motors mode :", err);
              });
            }

            function switchView(viewName) {
              document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
              document.getElementById('view-' + viewName).classList.add('active');

			  // --- Reset state "Select all" ---													   
              allSelected = false;
              let btnToggle = document.getElementById('btn-toggle-all');
              if (btnToggle) btnToggle.innerText = "Select all";

              let img = document.getElementById('videoStream');
              let videoContainer = document.getElementById('global-video-container');
              let sensorBox = document.getElementById('global-sensor-box');

              streamActive = false;
              img.src = "";

			  // Hiding video if on Diagnostic or Gallery page
              if (viewName === 'files' || viewName === 'diagnostic') {
                if (videoContainer) videoContainer.style.display = 'none';
                if (viewName === 'files') loadFileList(); 
              } else {
                if (videoContainer) videoContainer.style.display = 'block';
              }
            }

            async function triggerCapture(type) {
              let btnId = (type === 'hd') ? 'btn-hd' : 'btn-astro';
              let btn = document.getElementById(btnId);
              
              if (btn) btn.style.pointerEvents = 'none';
              lockingNavigation(true);

              try {
				// 1. Send capture request with profil
                let response = await fetch('/capture?type=' + type);
                if (!response.ok) {
                  let errText = await response.text();
                  alert("Erreur : " + errText);
                  return;
                }

				// 2. Active waiting for ESP32 to finish writing on SD card (via /status)																		   
                await waitForTaskCompletion();
				// 3. Update Gallery
                loadFileList();
                console.log("[UI] Capture " + type.toUpperCase() + " finished.");

              } catch (err) {
                console.error("Network error :", err);
              } finally {
				// 4. Giving back interface								
                if (btn) btn.style.pointerEvents = 'auto';
                lockingNavigation(false);
              }
            }

			// Polling function for ESP32 state
            function waitForTaskCompletion() {
              return new Promise((resolve) => {
                let checkInterval = setInterval(async () => {
                  try {
                    let res = await fetch('/status');
                    let data = await res.json();
                    
                    if (!data.busy) {
                      clearInterval(checkInterval);
                      resolve();
                    }
                  } catch (e) {
                    console.log("Temporary connection lost...");
                  }
                }, 500);
              });
            }

            function updateSensors() {
              fetch('/sensors')
              .then(res => res.json())
              .then(data => {
                document.getElementById('temp').classList.remove('error-text');
                document.getElementById('hum').classList.remove('error-text');
                document.getElementById('temp').innerText = data.temp.toFixed(1);
                document.getElementById('hum').innerText = data.hum.toFixed(1);
              }).catch(err => console.log(err));
            }

            setInterval(updateSensors, 15000);

            function loadFileList() {
              fetch('/list-files')
              .then(response => response.json())
              .then(files => {
                let listDiv = document.getElementById('file-list');
                listDiv.innerHTML = "";
                if (files.length === 0) {
                  listDiv.innerHTML = "<p>No file on SD card</p>";
                  return;
                }

                let controlsDiv = document.createElement('div');
                controlsDiv.style.marginBottom = "15px";
                controlsDiv.innerHTML = `
                <div>
                  <button id="btn-toggle-all" class="btn" style="padding:5px 10px; font-size:13px;" onclick="toggleSelectAll()">Select all</button>
                  <button class="btn" style="padding:5px 10px; font-size:13px; background:#28a745;" onclick="bulkAction('download')">Download selected</button>
                  <button class="btn btn-danger" style="padding:5px 10px; font-size:13px;" onclick="bulkAction('delete')">Delete selected</button>
                </div>
                <div id="downloadStatus" style="margin-top: 8px; font-size: 13px; color: #aaa;"></div>
                `;
                listDiv.appendChild(controlsDiv);

				// Generating files row with check box and file name
                files.forEach(file => {
                  let row = document.createElement('div');
                  row.className = 'file-row';
                  row.innerHTML = `
                  <label style="display: flex; align-items: center; cursor: pointer; width: 100%;">
                  <input type="checkbox" class="file-checkbox" value="${file}" style="margin-right: 10px;">
                  <span style="word-break: break-all; text-align: left;">${file}</span>
                  </label>
                  `;
                  listDiv.appendChild(row);
                });
              });
            }

            let allSelected = false;

            function toggleSelectAll() {
              allSelected = !allSelected;
              let checkboxes = document.querySelectorAll('.file-checkbox');
              checkboxes.forEach(cb => cb.checked = allSelected);
              let btn = document.getElementById('btn-toggle-all');
              if (btn) {
                btn.innerText = allSelected ? "Deselect all" : "Select all";
              }
            }

            async function bulkAction(action) {
              let selected = Array.from(document.querySelectorAll('.file-checkbox:checked')).map(cb => cb.value);
              if (selected.length === 0) {
                alert("Select at least one file.");
                return;
              }

              if (action === 'delete') {
                if (!confirm(`Delete all ${selected.length} selected files ?`)) return;
                for (let file of selected) {
                  await fetch('/delete?file=' + file);
                }
                loadFileList();
              } 
              else if (action === 'download') {
                const statusEl = document.getElementById('downloadStatus');
                const total = selected.length;
				        // Sequential loop to avoid ESP32 lock
                for (let i = 0; i < total; i++) {
                  let file = selected[i];
				          // Update progess indicator														  
                  if (statusEl) {
                    statusEl.innerText = `Downloading ${i + 1} / ${total}...`;
                  }

                  try {
                    let response = await fetch('/download?file=' + file);
                    if (!response.ok) {
                      console.error(`Error when downloading ${file}`);
                      continue;
                    }   

					        // Blob converting for binary response
                    let blob = await response.blob();
                    let url = window.URL.createObjectURL(blob);

					        // Creating a temporary invisible link to start local download
                    let a = document.createElement('a');
                    a.href = url;
                    a.download = file.startsWith('/') ? file.substring(1) : file;
                    document.body.appendChild(a);
                    a.click();

					        // Cleaning resource
                    document.body.removeChild(a);
                    window.URL.revokeObjectURL(url);

					        // Safety break between two heavy file
                    await new Promise(r => setTimeout(r, 300));
                  } catch (err) {
                    console.error("Erreur réseau :", err);
                  }
                }

                if (statusEl) {
                  statusEl.innerText = `Download complete (${total}/${total}) !`;
                  setTimeout(() => { if (statusEl) statusEl.innerText = ""; }, 3000);
                }
              }
            }

            function lockingNavigation(verrouille) {
			      // Deactivate/Activate navigation buttons															   
              ['nav-diag', 'nav-obs', 'nav-expl', 'nav-gallery'].forEach(id => {
                let btn = document.getElementById(id);
                if (btn) btn.disabled = verrouille;
              });
            }

            const zone = document.getElementById('joystick-zone');
            const handle = document.getElementById('joystick-handle');
            let dragging = false;
            let sendInterval = null;
            let currentThrottle = 0;
            let currentSteering = 0;

            zone.addEventListener('pointerdown', (e) => {
              dragging = true;
              
              if (!sendInterval) {
                sendInterval = setInterval(() => {
                  if (dragging && ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ throttle: currentThrottle, steering: currentSteering }));
                  }
                }, 50); // <--- Interval to send movement order
              }
            });

            zone.addEventListener('pointermove', (e) => {
              if (!dragging) return;
              let rect = zone.getBoundingClientRect();
              let centerX = rect.width / 2;
              let centerY = rect.height / 2;
              let x = e.clientX - rect.left - centerX;
              let y = e.clientY - rect.top - centerY;
              
              let dist = Math.sqrt(x*x + y*y);
              if (dist > 125) { 
                x *= 125 / dist; 
                y *= 125 / dist; 
              }
              
            // Visual update of joystick position on the page
              handle.style.transform = `translate(${x}px, ${y}px)`;
              
			      // Calculating Throttle (Y) and Steering (X)										
              currentThrottle = Math.round(-y * 1.6); 
              currentSteering = Math.round(x * 1.6);
            });

            function relacherJoystick() {
              if (!dragging) return;
              dragging = false;
              
              if (sendInterval) {
                clearInterval(sendInterval);
                sendInterval = null;
              }
              
			        // Centering back joystick on the page												
              handle.style.transform = `translate(0px, 0px)`;
              currentThrottle = 0;
              currentSteering = 0;
              
              // Immediate shut down notification via WebSocket
              if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ state: 'stop' }));
              }
            }

            window.addEventListener('pointerup', relacherJoystick);
            window.addEventListener('pointercancel', relacherJoystick);
          </script>
        </body>
      </html>
    )rawliteral");
  });

  // 4. Endpoint API for T° and Humidity value
   server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"temp\":" + String(temperatureData) + ",\"hum\":" + String(humidityData) + "}";
    request->send(200, "application/json", json);
  });

  // 5. Endpoint API for SD files listing
  server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;

    isRequesting = true; // Locking
    File root = SD.open("/");
    String json = "[";
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      bool first = true;
      while (file) {
        if (!file.isDirectory()) {
          String fileName = String(file.name());
          if (fileName.endsWith(".jpg")) {
            if (!first) json += ",";
            json += "\"" + fileName + "\"";
            first = false;
          }
        }
        file = root.openNextFile();
      }
    }
    json += "]";
    isRequesting = false; // Unlocking
    request->send(200, "application/json", json);
  });

  // 6. Endpoint for downloading files (Chunked)
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;

    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      if (!filename.startsWith("/")) filename = "/" + filename;
      
      if (SD.exists(filename)) {
        isRequesting = true; // Locking
        File file = SD.open(filename, FILE_READ);
        if (!file) {
          request->send(500, "text/plain", "File reading error");
          isRequesting = false; // Unlocking
          return;
        }

        AsyncWebServerResponse *response = request->beginChunkedResponse("image/jpeg", [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
          if (!file || !file.available()) {
            if (file) file.close();
            isRequesting = false; // Unlocking
            return 0;
          }
          size_t len = file.read(buffer, maxLen);
          yield(); 
          return len;
        });

        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        request->send(response);
        return;
      }
    }
    request->send(404, "text/plain", "File not found");
  });

  // 7. Endpoint for deleting files
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;

    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      if (!filename.startsWith("/")) filename = "/" + filename;
      if (SD.exists(filename)) {
        isRequesting = true; // Locking
        SD.remove(filename);
        isRequesting = false; // Unlocking
        request->send(200, "text/plain", "Deleted");
        return;
      }
    }
    request->send(404, "text/plain", "Deleting error");
  });

  // 8. Endpoint for snapshot
  server.on("/snapshot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;

    emptyCameraBuffer();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      request->send(500, "text/plain", "Failed to take a capture");
      return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse(200, "image/jpeg", fb->buf, fb->len);
    request->send(response);
    esp_camera_fb_return(fb);
  });

  // 9. Endpoint (unique) for captures
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request){
    if (currentMode != OBSERVATION) {
      request->send(400, "text/plain", "Incorrect mode");
      return;
    }
    
    if (isSystemBusy(request)) return;

	// Checking capture type with URL												  
    if (request->hasParam("type")) {
        String type = request->getParam("type")->value();
        isRequesting = true; // Locking
        
        if (type == "astro") {
            taskRequested = CAPTURE_ASTRO;
            request->send(200, "text/plain", "Astro capture accepted");
        } 
        else if (type == "hd") {
            taskRequested = CAPTURE_HD;
            request->send(200, "text/plain", "HD capture accepted");
        } 
        else {
            isRequesting = false; // Unlocking
            request->send(400, "text/plain", "Invalide type of capture");
        }
        return;
    }
    request->send(400, "text/plain", "Missing type of capture");
  });
  
  // 10. Endpoint SD card usage
  server.on("/sd-stats", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;

    isRequesting = true; // Locking
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    float percentage = (float)usedBytes / (float)totalBytes * 100.0;
    
    String json = "{\"total\":" + String(totalBytes) + ",\"used\":" + String(usedBytes) + ",\"percent\":" + String(percentage, 1) + "}";
    isRequesting = false; // Unlocking
    request->send(200, "application/json", json);
  });

  // 11. Endpoint modes manager
  server.on("/diagnostic", HTTP_GET, [](AsyncWebServerRequest *request){
    enterModeDiagnostic();
    request->send(200, "text/plain", "Mode Diagnostic activated");
  });

  server.on("/observation", HTTP_GET, [](AsyncWebServerRequest *request){
    enterModeObservation();
    request->send(200, "text/plain", "Mode Observation activated");
  });

  server.on("/exploration", HTTP_GET, [](AsyncWebServerRequest *request){
    enterModeExploration();
    request->send(200, "text/plain", "Mode Exploration activated");
  });
  
  server.on("/gallery", HTTP_GET, [](AsyncWebServerRequest *request){
    enterModeGallery();
    request->send(200, "text/plain", "Mode Gallery activated");
  });


  // 12. Endpoint for monitoring system state (busy or free)
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"busy\":" + String(isRequesting ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  // 13. Endpoint for motors management
  server.on("/toggleMotors", HTTP_GET, [](AsyncWebServerRequest *request){
    modeEquilibrium = !modeEquilibrium;
    Serial.print("[WEB] Equilibrium mode : ");
    Serial.println(modeEquilibrium ? "ACTIVE" : "INACTIVE");
    request->send(200, "text/plain", modeEquilibrium ? "ON" : "OFF");
  });

  // 14. Endpoints to test Radar
  server.on("/test-radar", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;
    int distance = webRadarDistanceStr();
    String json = "{\"distance\":" + String(distance) + "}";
    request->send(200, "application/json", json);
  });

  // 15. Endpoint to test MPU
  server.on("/test-mpu", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isSystemBusy(request)) return;
    float gravity = 0.0f, pitch = 0.0f, roll = 0.0f;
    if (webMpuData(gravity, pitch, roll)) {
      String json = "{";
      json += "\"pitch\":" + String(pitch, 1) + ",";
      json += "\"roll\":" + String(roll, 1) + ",";
      json += "\"gravity\":" + String(gravity, 2);
      json += "}";
      request->send(200, "application/json", json);
    } else {
      request->send(503, "text/plain", "MPU sensor not available");
    }
  });

  server.begin();
  Serial.println("[WEB] Server, SSE and WebSockets operational.");
}