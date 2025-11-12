# Preguntas y Respuestas sobre TLS y Temas Relacionados

## Pregunta 1: ¿Qué es el protocolo TLS, cuál es su importancia y qué es un certificado en ese contexto?

**Respuesta:**  
El protocolo **TLS (Transport Layer Security)** es un estándar criptográfico diseñado para proporcionar comunicaciones seguras en una red, como Internet, mediante el cifrado de datos, la garantía de integridad y la autenticación de las partes involucradas. Es el sucesor del protocolo SSL y se utiliza comúnmente en aplicaciones como HTTPS para proteger transacciones web.  

Su **importancia** radica en prevenir ataques como la interceptación de datos (*eavesdropping*), la manipulación de información (*tampering*) y la suplantación de identidad (*spoofing*), asegurando la **confidencialidad**, **integridad** y **autenticidad** de las comunicaciones.  

Un **certificado** en este contexto es un documento digital emitido por una autoridad confiable que vincula una **clave pública** con la identidad de un sitio web o entidad, permitiendo la verificación de la autenticidad del servidor y el establecimiento de una conexión segura.

---

## Pregunta 2: ¿A qué riesgos se está expuesto si no se usa TLS?

**Respuesta:**  
Sin TLS, las comunicaciones están expuestas a riesgos como:  
- **Interceptación de datos sensibles** (credenciales, información financiera, etc.) por atacantes en redes no seguras.  
- **Ataques de hombre en el medio (MITM)**, donde un tercero puede alterar o robar datos.  
- **Falta de autenticación**, permitiendo la suplantación de sitios web legítimos.  

Además, se pueden explotar vulnerabilidades en protocolos obsoletos o no cifrados, aumentando el riesgo de **brechas de seguridad**, **fugas de datos** y **ataques cibernéticos en general**.

---

## Pregunta 3: ¿Qué es un CA (Certificate Authority)?

**Respuesta:**  
Un **CA (Certificate Authority)** es una entidad confiable que **emite, firma y gestiona certificados digitales**, actuando como un **tercero de confianza** para validar la identidad de sitios web, empresas o individuos.  

Los CA verifican la propiedad de dominios o entidades antes de emitir certificados, lo que ayuda a establecer la **confianza en las comunicaciones seguras como TLS**.

---

## Pregunta 4: ¿Qué es una cadena de certificados y cuál es la vigencia promedio de los eslabones de la cadena?

**Respuesta:**  
Una **cadena de certificados** (*certificate chain*) es una secuencia jerárquica de certificados que comienza con un **certificado raíz (root CA)**, pasa por **certificados intermedios** y termina en el **certificado de entidad final (leaf certificate)**, donde cada uno firma al siguiente para establecer una **ruta de confianza**.  

**Vigencia promedio:**  
- **Certificados raíz:** 10–20 años  
- **Certificados intermedios:** 3–10 años  
- **Certificados de hoja (leaf):**  
  - Actualmente: hasta **398 días**  
  - A partir de **marzo de 2029**: máximo **47 días** (según propuestas de Google y la industria)

---

## Pregunta 5: ¿Qué es un keystore y qué es un certificate bundle?

**Respuesta:**  
- **Keystore:**  
  Un archivo o repositorio seguro que almacena **claves privadas**, **certificados** y, a veces, **claves públicas**. Se usa para gestionar credenciales criptográficas en aplicaciones como servidores web o clientes TLS.  

- **Certificate bundle:**  
  Una colección de certificados (generalmente la **cadena completa**: raíz + intermedios), **concatenados en un solo archivo** (formato PEM), para facilitar la instalación y validación en entornos TLS.

---

## Pregunta 6: ¿Qué es la autenticación mutua en el contexto de TLS?

**Respuesta:**  
La **autenticación mutua en TLS**, también conocida como **mTLS (mutual TLS)**, es un proceso donde **tanto el cliente como el servidor se autentican mutuamente** presentando y verificando **certificados digitales** durante el *handshake* TLS.  

Esto mejora la seguridad al requerir **identificación bidireccional**, común en APIs seguras, sistemas bancarios y redes internas.

---

## Pregunta 7: ¿Cómo se habilita la validación de certificados en el ESP32?

**Respuesta:**  
En el **ESP32 con Arduino IDE**, se habilita la validación de certificados usando la biblioteca `WiFiClientSecure`:  

```cpp
WiFiClientSecure client;
client.setCACert(root_ca_certificate);  // Certificado raíz en formato PEM
client.setCertificate(server_cert); // Certificado digital del cliente
client.setPrivateKey(private_key); // Certificado clave privada
client.loadCertificate() // Cargar certificados
client.verify() // Certificado de validez 
```
**Ejemplo:**
```cpp
const char* root_ca = "-----BEGIN CERTIFICATE-----\n...";
WiFiClientSecure client;
client.setCACert(root_ca);
```
---

## Pregunta 8: Si el sketch necesita conectarse a múltiples dominios con certificados generados por CAs distintos, ¿qué alternativas hay?

**Respuesta:**
Para mejorar la seguridad y compatibilidad en la verificación de certificados, se recomienda cargar un conjunto de certificados raíz en formato PEM. Esto permite que el cliente valide servidores que usen diferentes autoridades certificadoras.

### ¿Cómo hacerlo?

1. **Concatenar certificados raíz**  
   Junta todos los certificados raíz en un solo archivo o string en formato PEM. Cada certificado debe estar delimitado por:

**Ejemplo:**
```cpp
const char* ca_bundle = 
  "-----BEGIN CERTIFICATE-----\n...CA1...\n-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n...CA2...\n-----END CERTIFICATE-----\n";
client.setCACert(ca_bundle);
```

2. **Usar `setCACert()` en el cliente**  
En tu código C++, carga el bundle concatenado usando:

```cpp
client.setCACert(ca_bundle_pem);
```

Configurar `esp_tls_cfg_t` con un buffer que incluya múltiples CAs.

3. **Usar certificados por dominio (si se conoce de antemano)**
Crear instancias separadas de `WiFiClientSecure` con diferentes CAs.

**Evitar:** Deshabilitar la validación (`setInsecure()`), ya que expone a MITM.

---

## Pregunta 9: ¿Cómo se puede obtener el certificado para un dominio?

**Respuesta:**
Puedes obtener un certificado TLS de forma **gratuita o paga**, dependiendo de tus necesidades de seguridad, automatización y soporte.

### Métodos disponibles


| Método             | Descripción                                                                 |
|--------------------|------------------------------------------------------------------------------|
| Let's Encrypt      | Gratuito, automático vía protocolo ACME. Usa herramientas como Certbot.     |
| ZeroSSL            | Gratuito, con validación por email, DNS o HTTP.                             |
| Proveedores pagos  | DigiCert, Sectigo, GlobalSign. Ofrecen mayor soporte, garantías y flexibilidad. |

### Pasos para obtener un certificado con Let's Encrypt usando Certbot

```cpp
sudo certbot certonly --standalone -d tudominio.com
```

**Genera:** `fullchain.pem` (cadena completa) y `privkey.pem` (clave privada).

---

## Pregunta 10: ¿A qué se hace referencia cuando se habla de llave pública y privada en el contexto de TLS?

**Respuesta:**
### Llave Privada (private key)

- Es un **secreto exclusivo del propietario**.
- Se utiliza para:
  - **Descifrar** datos que fueron cifrados con la llave pública.
  - **Firmar digitalmente** mensajes para garantizar autenticidad.

### Llave Pública (public key)
- Se **comparte abiertamente**.
- Se utiliza para:
  - **Cifrar** datos que solo la llave privada puede descifrar.
  - **Verificar firmas** digitales hechas con la llave privada.

### Uso en TLS (Transport Layer Security)

Durante el proceso de **handshake TLS**:

1. El **servidor envía su certificado**, que incluye su llave pública.
2. El **cliente usa esa llave pública** para cifrar un **secreto de sesión**.
3. Solo el **servidor**, con su llave privada, puede **descifrar ese secreto**.
4. A partir de ese secreto compartido, ambos generan claves simétricas para cifrar la comunicación.

>  Este mecanismo garantiza confidencialidad, autenticidad y protección contra ataques de intermediarios (MITM).

---

## Pregunta 11: ¿Qué pasará con el código cuando los certificados expiren?

**Respuesta:**
Cuando un certificado **expira**:

- Las conexiones TLS **fallarán** con errores como:
  - `CERTIFICATE_VERIFY_FAILED`
  - `CERT_EXPIRED`

- En **ESP32**, el `client.connect()` retornará `false` o lanzará un error.
- En navegadores: aparece advertencia de seguridad.

**Solución:**
Actualizar el **certificado raíz o de servidor** en el código o bundle.

> Automatizar renovación con Let's Encrypt + OTA en ESP32.

---

## Pregunta 12: ¿Qué teoría matemática es el fundamento de la criptografía moderna? ¿Cuáles son las posibles implicaciones de la computación cuántica para los métodos de criptografía actuales?

**Respuesta:**

### Fundamento matemático:
La **criptografía moderna** se basa en la teoría de números y problemas computacionalmente difíciles:
- Factorización de enteros → RSA
- Logaritmo discreto → Diffie-Hellman, ECC

Estos problemas son "fáciles de calcular en una dirección, pero extremadamente difíciles en la inversa" con computadoras clásicas.

### Implicaciones de la computación cuántica:


| Algoritmo cuántico | Afecta a                          | Impacto                                      |
|--------------------|-----------------------------------|----------------------------------------------|
| Shor's             | RSA, ECC, Diffie-Hellman          | Rompe completamente la seguridad             |
| Grover's           | Cifrado simétrico (AES), hash     | Reduce la fuerza efectiva a la mitad         |

> Nota: La computación cuántica representa una amenaza significativa para los sistemas criptográficos actuales, especialmente los basados en factorización y logaritmos discretos.

**Consecuencias:**

- **RSA y ECC quedarán obsoletos** cuando existan computadoras cuánticas escalables.
- **AES-256 seguirá siendo seguro**, pero AES-128 será vulnerable.

**Solución: Criptografía post-cuántica (PQC)**
NIST está estandarizando algoritmos resistentes:

- **Kyber** (intercambio de claves)
- **Dilithium** (firmas digitales)

---

### Referencias

- [RFC 8446 - The Transport Layer Security (TLS) Protocol Version 1.3](https://datatracker.ietf.org/doc/html/rfc8446)
- [Mozilla - TLS Overview](https://wiki.mozilla.org/Security/Server_Side_TLS)
- [Let's Encrypt - How It Works](https://letsencrypt.org/how-it-works/)
- [OWASP - Transport Layer Protection Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Transport_Layer_Protection_Cheat_Sheet.html)
- [DigiCert - What is a Certificate Authority?](https://www.digicert.com/blog/what-is-a-certificate-authority)
- [GlobalSign - Certificate Chain](https://www.globalsign.com/en/blog/what-is-a-certificate-chain)
- [Google Security Blog - Reducing TLS Certificate Lifespans](https://security.googleblog.com/2020/03/modernizing-pki.html)
- [ESP32 Arduino - WiFiClientSecure](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/src/WiFiClientSecure.h)
- [ZeroSSL - Free Certificates](https://zerossl.com/free-ssl/#ssl-certificates)
- [NIST - Post-Quantum Cryptography](https://csrc.nist.gov/projects/post-quantum-cryptography)
- [Cloudflare - What happens when a certificate expires?](https://www.cloudflare.com/learning/ssl/what-happens-when-an-ssl-certificate-expires/)

---


