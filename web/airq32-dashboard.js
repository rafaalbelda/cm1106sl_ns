(() => {
  "use strict";

  const entities = [
    { key: "aqi", name: "European Air Quality Index", label: "AQI europeo", icon: "AQI", tone: aqiTone },
    { key: "co2", name: "CO2 Level", label: "CO₂", icon: "CO₂", tone: co2Tone },
    { key: "pm25", name: "PM2.5", label: "PM2.5", icon: "PM", tone: pm25Tone },
    { key: "pm10", name: "PM10", label: "PM10", icon: "PM", tone: pm10Tone },
    { key: "voc", name: "VOC Index", label: "Índice VOC", icon: "VOC", tone: vocTone },
    { key: "nox", name: "NOx Index", label: "Índice NOx", icon: "NOx", tone: noxTone },
    { key: "temp", name: "Temperatura", label: "Temperatura", icon: "°C", tone: neutralTone },
    { key: "humidity", name: "Humedad", label: "Humedad", icon: "%", tone: humidityTone },
    { key: "pressure", name: "Presión", label: "Presión", icon: "hPa", tone: neutralTone },
    { key: "pm1", name: "PM1.0", label: "PM1.0", icon: "PM", tone: neutralTone },
  ];

  const weatherEntities = [
    { key: "aemet_status", domain: "text_sensor", name: "AEMET Estado", label: "Estado de la actualización", icon: "↻", text: true },
    { key: "aemet_updated", domain: "text_sensor", name: "AEMET Última actualización", label: "Última actualización", icon: "◷", text: true },
    { key: "aemet_condition", domain: "text_sensor", name: "AEMET Estado del cielo", label: "Estado del cielo", icon: "AEMET", text: true },
    { key: "aemet_temp", domain: "sensor", name: "AEMET Temperatura", label: "Temperatura exterior", icon: "°C" },
    { key: "aemet_feels", domain: "sensor", name: "AEMET Sensación térmica", label: "Sensación térmica", icon: "°C" },
    { key: "aemet_high", domain: "sensor", name: "AEMET Temperatura máxima", label: "Máxima prevista", icon: "↑" },
    { key: "aemet_low", domain: "sensor", name: "AEMET Temperatura mínima", label: "Mínima prevista", icon: "↓" },
    { key: "aemet_rain", domain: "sensor", name: "AEMET Probabilidad de precipitación", label: "Probabilidad de lluvia", icon: "%" },
    { key: "aemet_humidity", domain: "sensor", name: "AEMET Humedad", label: "Humedad exterior", icon: "%" },
  ].map((entity) => ({ ...entity, tone: neutralTone, source: "AEMET" }));

  const allEntities = [...entities, ...weatherEntities];

  const state = Object.create(null);
  let municipalityName = "Tres Cantos";
  const aliases = new Map();
  const byKey = new Map(allEntities.map((entity) => [entity.key, entity]));

  allEntities.forEach((entity) => {
    aliases.set(normalize(entity.name), entity.key);
    aliases.set(normalize(slug(entity.name)), entity.key);
  });

  function normalize(value) {
    return String(value || "")
      .normalize("NFD")
      .replace(/[\u0300-\u036f]/g, "")
      .toLowerCase()
      .replace(/[^a-z0-9]/g, "");
  }

  function slug(value) {
    return value.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_|_$/g, "");
  }

  function tone(level, text) {
    return { level, text };
  }

  function neutralTone() { return tone("neutral", "Interior"); }
  function humidityTone(v) {
    if (v < 30) return tone("warn", "Ambiente seco");
    if (v > 70) return tone("warn", "Humedad alta");
    return tone("good", "Confortable");
  }
  function aqiTone(v) {
    if (v <= 25) return tone("good", "Muy bajo");
    if (v <= 50) return tone("fair", "Bajo");
    if (v <= 75) return tone("warn", "Medio");
    if (v <= 100) return tone("bad", "Alto");
    return tone("critical", "Muy alto");
  }
  function co2Tone(v) {
    if (v < 600) return tone("good", "Bueno");
    if (v < 1000) return tone("fair", "Aceptable");
    if (v <= 2000) return tone("warn", "Ventilar");
    return tone("bad", "Muy alto");
  }
  function vocTone(v) {
    if (v < 150) return tone("good", "Bueno");
    if (v < 250) return tone("fair", "Regular");
    if (v <= 400) return tone("warn", "Malo");
    return tone("bad", "Muy malo");
  }
  function noxTone(v) {
    if (v <= 1) return tone("good", "Limpio");
    if (v <= 50) return tone("fair", "Bueno");
    if (v <= 200) return tone("warn", "Presencia");
    return tone("bad", "Malo");
  }
  function pm25Tone(v) {
    if (v <= 10) return tone("good", "Bueno");
    if (v <= 20) return tone("fair", "Moderado");
    if (v <= 25) return tone("warn", "Elevado");
    return tone("bad", "Malo");
  }
  function pm10Tone(v) {
    if (v <= 20) return tone("good", "Bueno");
    if (v <= 40) return tone("fair", "Moderado");
    if (v <= 50) return tone("warn", "Elevado");
    return tone("bad", "Malo");
  }

  function entityUrl(domain, name, action = "") {
    const base = `/${domain}/${encodeURIComponent(name)}`;
    return action ? `${base}/${action}` : base;
  }

  async function getEntity(domain, name, detail = false) {
    const response = await fetch(`${entityUrl(domain, name)}${detail ? "?detail=all" : ""}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`${response.status} ${name}`);
    return response.json();
  }

  async function postEntity(domain, name, action, params = {}) {
    const query = new URLSearchParams(params).toString();
    const response = await fetch(`${entityUrl(domain, name, action)}${query ? `?${query}` : ""}`, { method: "POST" });
    if (!response.ok) throw new Error(`${response.status} ${name}`);
  }

  function appTemplate() {
    return `
      <div class="app-shell">
        <header class="topbar">
          <a class="brand" href="#dashboard" aria-label="AirQ32-W">
            <span class="brand-mark">AQ</span>
            <span><strong>AirQ32-W</strong><small>Monitor ambiental</small></span>
          </a>
          <span id="connection" class="connection offline"><i></i><span>Conectando</span></span>
        </header>

        <nav class="tabs" aria-label="Navegación principal">
          <a href="#dashboard" data-route="dashboard">Calidad del aire</a>
          <a href="#config" data-route="config">Configuración</a>
        </nav>

        <main>
          <section id="dashboard-view" class="view">
            <div class="hero tone-neutral" id="aqi-hero">
              <div>
                <span class="eyebrow">CALIDAD DEL AIRE INTERIOR</span>
                <h1 id="aqi-main">--</h1>
                <p id="aqi-caption">Esperando mediciones</p>
              </div>
              <div class="hero-orbit"><span>AQI</span></div>
            </div>

            <div id="ventilation" class="recommendation">
              <span class="recommendation-icon">↻</span>
              <div><strong>Analizando el ambiente</strong><small>La recomendación aparecerá al recibir datos.</small></div>
            </div>

            <div id="sensor-grid" class="sensor-grid">
              ${entities.map(cardTemplate).join("")}
            </div>
            <div class="group-heading"><span class="eyebrow">PREDICCIÓN OFICIAL</span><h2 id="weather-location">AEMET · Tres Cantos</h2></div>
            <div id="weather-grid" class="sensor-grid weather-grid">
              ${weatherEntities.map(cardTemplate).join("")}
            </div>
            <p class="updated">Actualización en tiempo real · <span id="last-update">sin datos</span></p>
          </section>

          <section id="config-view" class="view" hidden>
            <div class="section-heading">
              <span class="eyebrow">AJUSTES DEL DISPOSITIVO</span>
              <h1>Configuración</h1>
              <p>Los cambios se aplican directamente en el AirQ32-W.</p>
            </div>

            <div class="settings-grid">
              <article class="setting-card">
                <div><span class="setting-icon">▣</span><strong>Página de la pantalla</strong><small>Cambia la vista mostrada en la pantalla física.</small></div>
                <select id="display-page" aria-label="Página de la pantalla">
                  <option>Calidad del aire</option><option>Resumen</option><option>Ventilación</option>
                  <option>Partículas</option><option>CO2</option><option>VOC y NOx</option>
                  <option>Comparación AQI</option><option>Tiempo</option>
                </select>
              </article>

              <article class="setting-card">
                <div><span class="setting-icon">☼</span><strong>Brillo de la pantalla</strong><small>Ajusta la retroiluminación.</small></div>
                <div class="range-row"><input id="brightness" type="range" min="0" max="255" value="255"><output id="brightness-value">100%</output></div>
              </article>

              <article class="setting-card">
                <div><span class="setting-icon">±</span><strong>Offset de temperatura</strong><small>Compensa el calor generado por el dispositivo.</small></div>
                <div class="number-row"><input id="temperature-offset" type="number" min="-20" max="20" step="0.1"><span>°C</span><button id="save-offset">Guardar</button></div>
              </article>

              <article class="setting-card secret-card">
                <div><span class="setting-icon">⌁</span><strong>Clave API de AEMET</strong><small>Introduce una clave nueva. La clave guardada nunca se muestra en esta página.</small></div>
                <div class="secret-row"><input id="aemet-api-key" type="password" minlength="50" maxlength="400" autocomplete="new-password" spellcheck="false" placeholder="Nueva clave de AEMET"><button id="save-aemet-api-key">Guardar</button></div>
              </article>

              <article class="setting-card">
                <div><span class="setting-icon">◷</span><strong>Intervalo de AEMET</strong><small>Frecuencia de consulta automática, entre 1 minuto y 24 horas.</small></div>
                <div class="number-row"><input id="aemet-update-interval" type="number" min="1" max="1440" step="1" value="5"><span>min</span><button id="save-aemet-interval">Guardar</button></div>
              </article>

              <article class="setting-card municipality-card">
                <div><span class="setting-icon">⌖</span><strong>Localidad de AEMET</strong><small>Localidad actual: <span id="current-aemet-municipality">Tres Cantos</span></small></div>
                <div class="wifi-fields">
                  <div class="secret-row"><input id="aemet-municipality-query" type="text" maxlength="80" autocomplete="off" placeholder="Nombre exacto o código INE"><button id="search-aemet-municipality">Buscar y guardar</button></div>
                  <small id="aemet-municipality-status">La búsqueda consulta el catálogo oficial de AEMET.</small>
                </div>
              </article>

              <article class="setting-card wifi-card">
                <div><span class="setting-icon">⌁</span><strong>Conexión Wi-Fi</strong><small>Red actual: <span id="current-wifi-ssid">cargando…</span></small></div>
                <div class="wifi-fields">
                  <input id="wifi-ssid" type="text" maxlength="32" autocomplete="off" spellcheck="false" placeholder="Nombre de la nueva red (SSID)">
                  <div class="secret-row"><input id="wifi-password" type="password" maxlength="64" autocomplete="new-password" spellcheck="false" placeholder="Contraseña (vacía para red abierta)"><button id="save-wifi">Conectar y guardar</button></div>
                  <small id="wifi-config-status">Al cambiar de red se perderá temporalmente la conexión con esta página.</small>
                </div>
              </article>

              <article class="setting-card info-card">
                <div><span class="setting-icon">i</span><strong>Acceso local</strong><small>Esta web y los sensores locales no necesitan Home Assistant. La predicción de AEMET sí requiere conexión a Internet.</small></div>
                <dl><dt>Dirección</dt><dd id="device-address">${location.host}</dd><dt>Estado</dt><dd id="device-status">Conectando</dd></dl>
              </article>

              <article class="setting-card">
                <div><span class="setting-icon">↻</span><strong>Predicción de AEMET</strong><small>Fuerza una actualización de la predicción oficial para la localidad configurada.</small></div>
                <button class="primary-button" id="refresh-weather">Actualizar ahora</button>
              </article>
            </div>
          </section>
        </main>

        <div id="toast" role="status" aria-live="polite"></div>
      </div>`;
  }

  function cardTemplate(entity) {
    return `<article class="sensor-card tone-neutral" id="card-${entity.key}">
      <div class="card-icon">${entity.icon}</div>
      <div class="card-copy"><span>${entity.label}</span><strong id="value-${entity.key}">--</strong><small id="status-${entity.key}">Esperando datos</small></div>
    </article>`;
  }

  function route() {
    const current = location.hash === "#config" ? "config" : "dashboard";
    document.querySelectorAll("[data-route]").forEach((link) => link.classList.toggle("active", link.dataset.route === current));
    document.getElementById("dashboard-view").hidden = current !== "dashboard";
    document.getElementById("config-view").hidden = current !== "config";
  }

  function findKey(data) {
    const candidates = [data.name, data.name_id, data.id].filter(Boolean);
    for (const candidate of candidates) {
      const text = String(candidate);
      const lastSlash = text.includes("/") ? text.substring(text.lastIndexOf("/") + 1) : text;
      const withoutDomain = lastSlash.replace(/^(sensor|number|select|light|text_sensor|binary_sensor)-/, "");
      const key = aliases.get(normalize(withoutDomain));
      if (key) return key;
    }
    return null;
  }

  function updateSensor(key, data) {
    const entity = byKey.get(key);
    if (!entity) return;
    if (entity.text) {
      state[key] = data.value || data.state || "";
      const card = document.getElementById(`card-${key}`);
      card.className = "sensor-card tone-neutral";
      const weatherLabels = { sunny: "Soleado", partlycloudy: "Intervalos nubosos", cloudy: "Nublado", rainy: "Lluvia", pouring: "Lluvia fuerte", "lightning-rainy": "Tormenta", snowy: "Nieve", fog: "Niebla", "clear-night": "Despejado" };
      document.getElementById(`value-${key}`).textContent = weatherLabels[data.state] || data.state || "--";
      document.getElementById(`status-${key}`).textContent = entity.source === "AEMET" ? `AEMET · ${municipalityName}` : entity.source;
      if (key === "aemet_status") document.getElementById("aemet-municipality-status").textContent = data.state || "Sin estado";
      return;
    }
    const numeric = Number(data.value);
    state[key] = Number.isFinite(numeric) ? numeric : NaN;
    const currentTone = Number.isFinite(numeric) ? entity.tone(numeric) : tone("neutral", "Sin datos");
    const card = document.getElementById(`card-${key}`);
    card.className = `sensor-card tone-${currentTone.level}`;
    document.getElementById(`value-${key}`).textContent = data.state || "--";
    document.getElementById(`status-${key}`).textContent = entity.source === "AEMET" ? `AEMET · ${municipalityName}` : (entity.source || currentTone.text);
    document.getElementById("last-update").textContent = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
    updateSummary();
  }

  function setMunicipality(name) {
    if (!name) return;
    municipalityName = name;
    document.getElementById("weather-location").textContent = `AEMET · ${name}`;
    document.getElementById("current-aemet-municipality").textContent = name;
    weatherEntities.forEach((entity) => {
      const status = document.getElementById(`status-${entity.key}`);
      if (status) status.textContent = `AEMET · ${name}`;
    });
  }

  function updateSummary() {
    const value = state.aqi;
    const summaryTone = Number.isFinite(value) ? aqiTone(value) : tone("neutral", "Esperando mediciones");
    const hero = document.getElementById("aqi-hero");
    hero.className = `hero tone-${summaryTone.level}`;
    document.getElementById("aqi-main").textContent = Number.isFinite(value) ? Math.round(value) : "--";
    document.getElementById("aqi-caption").textContent = summaryTone.text;

    const ventilate = state.co2 > 1000 || state.voc > 200 || state.nox > 50;
    const ready = [state.co2, state.voc, state.nox].some(Number.isFinite);
    const panel = document.getElementById("ventilation");
    if (!ready) return;
    panel.className = `recommendation ${ventilate ? "alert" : "ok"}`;
    panel.innerHTML = ventilate
      ? '<span class="recommendation-icon">↗</span><div><strong>Conviene ventilar</strong><small>Uno o más indicadores interiores están elevados.</small></div>'
      : '<span class="recommendation-icon">✓</span><div><strong>Ventilación normal</strong><small>CO₂, VOC y NOx están dentro de sus rangos recomendados.</small></div>';
  }

  function setConnection(connected) {
    const element = document.getElementById("connection");
    element.className = `connection ${connected ? "online" : "offline"}`;
    element.querySelector("span").textContent = connected ? "En directo" : "Reconectando";
    document.getElementById("device-status").textContent = connected ? "Conectado" : "Reconectando";
  }

  async function loadSensors() {
    await Promise.allSettled(allEntities.map(async (entity) => {
      const data = await getEntity(entity.domain || "sensor", entity.name);
      updateSensor(entity.key, data);
    }));
  }

  async function loadSettings() {
    const results = await Promise.allSettled([
      getEntity("select", "Página de pantalla", true),
      getEntity("number", "Offset temperatura", true),
      getEntity("light", "Display Backlight", true),
      getEntity("number", "Intervalo AEMET", true),
      getEntity("text_sensor", "WiFi SSID actual", true),
      getEntity("text_sensor", "Estado configuración WiFi", true),
      getEntity("text_sensor", "AEMET Municipio", true),
    ]);
    if (results[0].status === "fulfilled") document.getElementById("display-page").value = results[0].value.value;
    if (results[1].status === "fulfilled") document.getElementById("temperature-offset").value = results[1].value.value;
    if (results[2].status === "fulfilled") {
      const brightness = results[2].value.state === "OFF" ? 0 : Number(results[2].value.brightness ?? 255);
      setBrightnessUi(brightness);
    }
    if (results[3].status === "fulfilled") document.getElementById("aemet-update-interval").value = results[3].value.value;
    if (results[4].status === "fulfilled") document.getElementById("current-wifi-ssid").textContent = results[4].value.state || "sin conexión";
    if (results[5].status === "fulfilled" && results[5].value.state) document.getElementById("wifi-config-status").textContent = results[5].value.state;
    if (results[6].status === "fulfilled") setMunicipality(results[6].value.state);
  }

  function connectEvents() {
    const source = new EventSource("/events");
    source.onopen = () => setConnection(true);
    source.onerror = () => setConnection(false);
    const receive = (event) => {
      try {
        const data = JSON.parse(event.data);
        const identifiers = [data.name, data.name_id, data.id].filter(Boolean).map(normalize);
        if (identifiers.some((value) => value.includes(normalize("AEMET Municipio")))) {
          setMunicipality(data.state || data.value);
          return;
        }
        const key = findKey(data);
        if (key) updateSensor(key, data);
      } catch (_) { /* Ignore incomplete diagnostic events. */ }
    };
    source.addEventListener("state", receive);
    source.addEventListener("state_detail_all", receive);
  }

  let toastTimer;
  function notify(message, error = false) {
    const toast = document.getElementById("toast");
    toast.textContent = message;
    toast.className = error ? "show error" : "show";
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toast.className = ""; }, 2600);
  }

  function setBrightnessUi(value) {
    const safe = Math.max(0, Math.min(255, Number(value) || 0));
    document.getElementById("brightness").value = safe;
    document.getElementById("brightness-value").value = `${Math.round(safe / 2.55)}%`;
  }

  function bindSettings() {
    document.getElementById("display-page").addEventListener("change", async (event) => {
      try {
        await postEntity("select", "Página de pantalla", "set", { option: event.target.value });
        notify("Página de pantalla actualizada");
      } catch (_) { notify("No se pudo cambiar la página", true); }
    });

    let brightnessTimer;
    document.getElementById("brightness").addEventListener("input", (event) => {
      setBrightnessUi(event.target.value);
      clearTimeout(brightnessTimer);
      brightnessTimer = setTimeout(async () => {
        const brightness = Number(event.target.value);
        try {
          if (brightness === 0) await postEntity("light", "Display Backlight", "turn_off");
          else await postEntity("light", "Display Backlight", "turn_on", { brightness });
        } catch (_) { notify("No se pudo ajustar el brillo", true); }
      }, 180);
    });

    document.getElementById("save-offset").addEventListener("click", async () => {
      const value = Number(document.getElementById("temperature-offset").value);
      if (!Number.isFinite(value) || value < -20 || value > 20) return notify("El offset debe estar entre -20 y 20 °C", true);
      try {
        await postEntity("number", "Offset temperatura", "set", { value });
        notify("Offset de temperatura guardado");
      } catch (_) { notify("No se pudo guardar el offset", true); }
    });

    document.getElementById("save-aemet-api-key").addEventListener("click", async () => {
      const input = document.getElementById("aemet-api-key");
      const apiKey = input.value.trim();
      if (apiKey.length < 50 || apiKey.length > 400) {
        return notify("La clave de AEMET debe tener entre 50 y 400 caracteres", true);
      }
      try {
        await postEntity("text", "Clave API AEMET 1", "set", { value: apiKey.slice(0, 200) });
        await postEntity("text", "Clave API AEMET 2", "set", { value: apiKey.slice(200) });
        await postEntity("button", "Guardar clave API AEMET", "press");
        input.value = "";
        notify("Clave de AEMET guardada; actualización iniciada");
      } catch (_) {
        notify("No se pudo guardar la clave de AEMET", true);
      }
    });

    document.getElementById("save-aemet-interval").addEventListener("click", async () => {
      const input = document.getElementById("aemet-update-interval");
      const value = Number(input.value);
      if (!Number.isInteger(value) || value < 1 || value > 1440) {
        return notify("El intervalo debe ser un número entero entre 1 y 1440 minutos", true);
      }
      try {
        await postEntity("number", "Intervalo AEMET", "set", { value });
        notify(`Intervalo de AEMET guardado: ${value} min`);
      } catch (_) {
        notify("No se pudo guardar el intervalo de AEMET", true);
      }
    });

    document.getElementById("search-aemet-municipality").addEventListener("click", async () => {
      const input = document.getElementById("aemet-municipality-query");
      const query = input.value.trim();
      const length = new TextEncoder().encode(query).length;
      if (length < 2 || length > 80) {
        return notify("Introduce un nombre o código INE válido", true);
      }
      try {
        await postEntity("text", "Buscar municipio AEMET", "set", { value: query });
        await postEntity("button", "Buscar municipio AEMET", "press");
        input.value = "";
        document.getElementById("aemet-municipality-status").textContent = "Consultando el catálogo de AEMET…";
        notify("Búsqueda de localidad iniciada");
      } catch (_) {
        notify("No se pudo iniciar la búsqueda de localidad", true);
      }
    });

    document.getElementById("save-wifi").addEventListener("click", async () => {
      const ssidInput = document.getElementById("wifi-ssid");
      const passwordInput = document.getElementById("wifi-password");
      const ssid = ssidInput.value;
      const password = passwordInput.value;
      const encoder = new TextEncoder();
      const ssidLength = encoder.encode(ssid).length;
      const passwordLength = encoder.encode(password).length;
      if (ssidLength < 1 || ssidLength > 32) {
        return notify("El SSID debe ocupar entre 1 y 32 bytes", true);
      }
      if (passwordLength !== 0 && (passwordLength < 8 || passwordLength > 64)) {
        return notify("La contraseña debe estar vacía o tener entre 8 y 64 bytes", true);
      }

      let connectionRequested = false;
      try {
        await postEntity("text", "Nuevo SSID WiFi", "set", { value: ssid });
        await postEntity("text", "Nueva contraseña WiFi", "set", { value: password });
        connectionRequested = true;
        document.getElementById("wifi-config-status").textContent = "Conectando a la nueva red…";
        notify("Cambiando la conexión Wi-Fi…");
        await postEntity("button", "Guardar configuración WiFi", "press");
        ssidInput.value = "";
        passwordInput.value = "";
      } catch (_) {
        if (connectionRequested) {
          ssidInput.value = "";
          passwordInput.value = "";
          notify("Cambio enviado; vuelve a abrir el dispositivo en la nueva red");
        } else {
          notify("No se pudo enviar la configuración Wi-Fi", true);
        }
      }
    });

    document.getElementById("refresh-weather").addEventListener("click", async () => {
      try {
        await postEntity("button", "Actualizar AEMET", "press");
        notify("Actualización de AEMET iniciada");
      } catch (_) { notify("No se pudo iniciar la actualización", true); }
    });
  }

  function init() {
    document.documentElement.lang = "es";
    document.title = "AirQ32-W";
    document.body.innerHTML = appTemplate();
    window.addEventListener("hashchange", route);
    route();
    bindSettings();
    loadSensors();
    loadSettings();
    connectEvents();
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
