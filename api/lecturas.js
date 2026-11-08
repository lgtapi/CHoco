// API de lecturas del prototipo CHOCO
//   POST /api/lecturas  -> el ESP32 envía { lluvia, humedad, temperatura }
//   GET  /api/lecturas  -> la landing pide la última lectura y el historial
//
// Almacenamiento: Upstash Redis (se agrega gratis desde Vercel > Storage).
// Si no está configurado, guarda en memoria (sirve para probar, pero se
// borra cuando Vercel reinicia la función).

const UMBRAL_LLUVIA = 500;   // mismo umbral que en el código del ESP32
const MAX_HISTORIAL = 60;
const CLAVE = "choco:lecturas";

const REDIS_URL = process.env.KV_REST_API_URL || process.env.UPSTASH_REDIS_REST_URL;
const REDIS_TOKEN = process.env.KV_REST_API_TOKEN || process.env.UPSTASH_REDIS_REST_TOKEN;
const TOKEN_DISPOSITIVO = process.env.API_TOKEN || process.env.CHOCO_TOKEN; // opcional

const memoria = globalThis.__chocoMemoria || (globalThis.__chocoMemoria = []);

async function redis(comando) {
  const r = await fetch(REDIS_URL, {
    method: "POST",
    headers: { Authorization: `Bearer ${REDIS_TOKEN}`, "Content-Type": "application/json" },
    body: JSON.stringify(comando),
  });
  if (!r.ok) throw new Error(`Redis respondió ${r.status}`);
  return (await r.json()).result;
}

async function guardar(lectura) {
  if (REDIS_URL && REDIS_TOKEN) {
    await redis(["LPUSH", CLAVE, JSON.stringify(lectura)]);
    await redis(["LTRIM", CLAVE, 0, MAX_HISTORIAL - 1]);
  } else {
    memoria.unshift(lectura);
    memoria.length = Math.min(memoria.length, MAX_HISTORIAL);
  }
}

async function leer() {
  if (REDIS_URL && REDIS_TOKEN) {
    const filas = (await redis(["LRANGE", CLAVE, 0, MAX_HISTORIAL - 1])) || [];
    return filas.map((f) => JSON.parse(f));
  }
  return memoria;
}

function numero(v) {
  if (v === null || v === undefined || v === "") return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type, x-api-token, x-choco-token, Authorization");
  res.setHeader("Cache-Control", "no-store");
  if (req.method === "OPTIONS") return res.status(204).end();

  try {
    if (req.method === "POST") {
      const tokenRecibido = req.headers["x-api-token"] || req.headers["x-choco-token"] ||
        (req.headers.authorization || "").replace(/^Bearer\s+/i, "");
      if (TOKEN_DISPOSITIVO && tokenRecibido !== TOKEN_DISPOSITIVO) {
        return res.status(401).json({ error: "Token del dispositivo incorrecto." });
      }
      let cuerpo = req.body;
      if (typeof cuerpo === "string") cuerpo = JSON.parse(cuerpo || "{}");
      const lluvia = numero(cuerpo?.lluvia ?? cuerpo?.nivelAgua);
      if (lluvia === null) {
        return res.status(400).json({ error: "Falta el campo 'lluvia' (lectura analógica del sensor)." });
      }
      const lectura = {
        lluvia,
        humedad: numero(cuerpo.humedad),
        temperatura: numero(cuerpo.temperatura),
        llueve: lluvia > UMBRAL_LLUVIA,
        fecha: new Date().toISOString(),
      };
      await guardar(lectura);
      return res.status(201).json({ ok: true, lectura });
    }

    if (req.method === "GET") {
      const historial = await leer();
      return res.status(200).json({
        ultima: historial[0] || null,
        historial,
        umbral: UMBRAL_LLUVIA,
        almacenamiento: REDIS_URL && REDIS_TOKEN ? "redis" : "memoria",
      });
    }

    return res.status(405).json({ error: "Usa GET o POST." });
  } catch (e) {
    return res.status(500).json({ error: e.message });
  }
}
