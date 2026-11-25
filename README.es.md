[English](README.md) · **Español**

> **Última actualización:** 2026-11-23 · base: [README.md](README.md) `v1.0.20`
<!-- readme-sync: source-blob=4f5be15ad7624cb1a931543695e113c1944d83eb version=1.0.21 synced=2026-11-25 -->

# TumatraPDF

Un lector de PDF para sesiones largas y un motor de flashcards integrado (en desarrollo).

## 📹 Míralo en acción

[![Presentación de TumatraPDF](https://img.youtube.com/vi/1lx40vES_sk/maxresdefault.jpg)](https://youtu.be/1lx40vES_sk)

**▶ Ver la presentación:** [Tumatra PDF v120 with Flashcards](https://youtu.be/1lx40vES_sk) — un recorrido por la app para sesiones largas de lectura y las flashcards integradas.

## Créditos

Este proyecto es un fork de **SumatraPDF** — un lector de PDF ligero, rápido y de código abierto para Windows.

- **Repositorio original:** [github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- **Autor principal:** [Krzysztof Kowalczyk (kjk)](https://github.com/kjk)
- **Versión base utilizada:** Pre-release `3.7.20958`

## 🆕 Funciones

Las funciones de abajo fueron desarrolladas sobre SumatraPDF y **no existen en la versión original**. Siguen la misma filosofía del proyecto: código nativo en C/C++, interfaz Win32, sin dependencias externas.

> **Nota para Krzysztof Kowalczyk** (creador de SumatraPDF): estas funciones se implementaron como **parches directos en `sumatrapdf-src/src/`** — el código upstream se conserva en `sumatrapdf-src/`. Los directorios `src/features/` y `src/hooks/` **no existen**. Puedes incorporar cualquiera de ellas a SumatraPDF oficial sin problema.

---

### Lectura

#### EYE STrESS
Controles de brillo y contraste para leer sesiones largas en pantalla.
* `Invert`: ya existía en la aplicación original. Solo se agregó un botón de encendido/apagado.
* `Contrast` `+` `-`: **función nueva**:
	* **PDF**: agrega un filtro negro ajustable sobre todas las páginas
	* **Markdown**: cambia el color de las letras a tonos más oscuros
#### AutoScroll con temporizador y ETA
- `[] Timer` `Autoscroll` `+` `-` Desplazamiento automático continuo, con velocidad ajustable y temporizador.
	- `F9`: iniciar/detener
	- `F7 (bajar velocidad) / F8 (subir velocidad)`: ajustes rápidos
- `ETA`: tiempo estimado para terminar el libro según la velocidad actual
- Mantener `Ctrl`: pausa rápida → se reanuda al soltar
- `[x]` 5m `Timer`: ingresa el tiempo que quieres que dure tu sesión de lectura

> `Autoscroll:on` alimenta los _datos del perfil_ con tu tiempo de lectura, que se refleja en `Home`/`Graphs`
> La última velocidad usada se conserva para cada documento
> Funciona en PDF y Markdown

También funciona junto con otras herramientas: `Two Columns:on` `Trim:on` `Invert` `Contrast`, etc.
#### Two Columns
División rápida de documentos en dos columnas para lectura continua a una sola columna.
* `Two Columns`: divide cada página en dos páginas virtuales izquierda/derecha y las apila en orden vertical, así no tienes que desplazarte horizontalmente.

También funciona junto con otras herramientas: `Autoscroll:on` `Trim:on` `Invert` `Contrast`, etc.

> Cuando `Autoscroll:on`
* Mantener `Shift`: detiene rápidamente el _autoscroll_ y vuelve al diseño original de la página para que puedas leer gráficos o tablas de página completa entre las columnas (diseño muy común en libros educativos)
- Mantener `Ctrl`: pausa rápida → se reanuda al soltar
#### Trim
* `Trim`: elimina rápidamente los espacios vacíos al inicio y al final de las páginas, optimizando la lectura cuando los botones `Autoscroll` / `Two Columns` están activos.
* `Trim Config`: dibuja _líneas rojas_ arrastrables para ajustar rápido los límites de la página.
- Se recuerda por documento; los archivos nuevos empiezan sin trim.

También funciona junto con otras herramientas: `Two Columns:on` `Autoscroll:on` `Invert` `Contrast`, etc.
#### Hold

- `Hold`: guarda rápidamente tu posición de lectura para que puedas ir a otra sesión del libro y volver justo donde te detuviste.

### FLASHCARDS
Tarjetas estilo Anki creadas dentro de documentos PDF.
#### Creación
Crea tarjetas rápido (estilo cloze) dentro de PDFs. Dibuja una máscara de resaltado sobre el texto o la región del PDF.
- `Selección de texto`→`botón flotante de flashcard (tecla 's')`: convierte selecciones de texto en tarjetas (estilo cloze)
- Mantener `Ctrl` + `arrastrar un rectángulo` → `tecla s`: convierte el rectángulo en una tarjeta (estilo ocultamiento de imagen)
- `Alt`: permite varias selecciones de texto/rectángulo → `tecla s` → una sola tarjeta con el grupo
- `Ctrl` + `clic` en el resaltado de una flashcard: abre la _lista de flashcards_

#### Barra de herramientas FLASHCARDS
Barra secundaria que te permite estudiar las tarjetas dentro de este PDF. Algoritmo de repetición espaciada SM-2.
- `Study`: inicia el renderizado de máscaras y el proceso de evaluación. Califica las tarjetas con los números `1-4`.
- `1`: Otra vez | `2`: Difícil | `3`: Bien | `4`: Fácil
	- Calificar las tarjetas cambia el tiempo de repaso para las próximas sesiones.
	- Es recomendable buscar información sobre `spaced repetition` y `Anki`.
- `Delete`: elimina la _tarjeta_ activa
- `Order`: modifica el orden de las tarjetas _repasadas_ y _nuevas_
- `List`: tarjetas dentro de este documento
- `Filter`: repasa tarjetas de una región específica o de un capítulo (marcadores) de un documento, O estudia entre varios documentos abiertos en la misma **sesión**
- `Import`:
	- convierte resaltados normales en flashcards (los duplica para conservar el formato original)
	- importa tarjetas del **mismo documento** editadas por otra persona. Puedes dividir las tarjetas para trabajar con tus compañeros de estudio e importar las suyas en un único archivo compartido.
- `Session`: muestra los últimos documentos con tarjetas en los que estudiaste recientemente

> Las flashcards usan el mecanismo normal de resaltados, así que cualquier tarjeta guardada dentro de un documento sigue siendo un PDF normal y se puede abrir en cualquier otro programa
> Puedes 'ver' las regiones marcadas como flashcards en otros programas de PDF, pero solo podrás estudiarlas dentro de `Tumatra PDF`

El historial de estudio se guarda dentro de una carpeta `Profile Data` que puedes cambiar en `Profile Sync`. También puedes ubicarla en una carpeta sincronizada en la nube, como _Google Drive_, y compartirla entre varias PCs (solo Windows por ahora — multiplataforma es un objetivo futuro).
### Pestañas y sesiones ⭕🟢

#### SESIONES
Sincroniza _sesiones_ para abrir rápido varios documentos que usas con frecuencia en distintas sesiones de estudio. Restaura cualquier grupo al iniciar o cuando quieras; los grupos se sincronizan automáticamente al agregar, quitar o cerrar pestañas.
- `⭕`: sesión no sincronizada. Cualquier libro abierto aquí no se recuperará si cierras la ventana.
- `🟢`: sesión sincronizada. Los libros de esta sesión se recordarán cuando vuelvas a abrirla.
- `New Session`: crea una nueva sesión sincronizada para recuperar tus archivos habituales y guardar el tiempo de lectura y de estudio de flashcards dentro de esta sesión.
- `Clic en ⭕/🟢` → abre la lista desplegable → `clic derecho`:
	- `Open in new window`: abre una ventana nueva con todos los libros sincronizados
	- `Rename`
	- `Delete`
- `Ctrl+N` inicia una ventana no sincronizada.
- `Alt+clic` en las pestañas para cerrar documentos — el botón `[x]` fue eliminado
### Estadísticas y perfil

#### Estadísticas de lectura

- La lectura con `autoscroll` se registra por documento y por día, y se muestra en un panel `Home`.
- Ranking de más leídos, totales de Hoy/Total, mapa de calor de 365 días y gráfico anual; el total maestro sobrevive al renombrar o mover un libro.
#### Memoria y sincronización del perfil

- Todo el tiempo de lectura, los datos de flashcards y el estado por libro en un único `tumatraPDF-data_1.json` que se puede exportar, importar y sincronizar (por ejemplo, con Google Drive).
- El botón "🔁 Sync" de `Home` abre la ventana `Profile Sync`; 9 copias de seguridad locales rotativas; el ajuste `profileDataDir` apunta a una carpeta sincronizada; importación de portable→instalado.
- `Profile Sync`: define la carpeta de datos (una sola raíz para perfil + copias + estadísticas + estudio), fusión correcta (conserva el total diario mayor y el estado más nuevo por tarjeta), interfaz honesta con una línea de estado y copias de seguridad navegables por teclado.
### Edición

#### Edición de imágenes (clic derecho sobre una imagen)

- `Replace Image`, `Edit With Paint`, `Crop/Resize` y `Copy Image` al portapapeles — se muestran solo cuando hay una imagen bajo el cursor.
- Los cambios de `Crop/Resize` se escriben de vuelta en el PDF al guardar; la proporción se conserva arrastrando con `Shift`.

#### Edit mode

- Pega una imagen del portapapeles en la página actual y ubícala con un clic; arrastra los tiradores para moverla o recortarla; `Apply` escribe los cambios en el PDF.
- Se activa y desactiva desde la barra de herramientas; es mutuamente excluyente con `Arch Tools` y `Flashcards`.

#### Capture tool (Shift+S)

- Dibuja un rectángulo sobre la página para capturarla al DPI del documento y copiarla al portapapeles.
- Funciona en la página donde empezó el arrastre; se cierra automáticamente después de capturar; `Esc` cancela.
#### Arch Tools (escala y medición)

- Para PDFs de arquitectura: dibuja una línea, ingresa su longitud real y luego mide x/y/longitud en unidades reales.
- `Radius` (arco de 3 clics) y `Ref Line` (línea guía de página completa); borra líneas individuales con `E`.
- Las restricciones de conflicto evitan que `Arch Tools`, `Flashcards`, `AutoScroll`, `Scan Mode` y `Trim` se superpongan.
#### Barra lateral de marcadores editable

- Agrega marcadores para la página actual; con clic derecho puedes `Rename`, `Delete` o `Update`; arrastra para reordenar y aplicar sangría con una línea guía blanca.
- Los cambios se guardan en el PDF al guardar y se conservan al reabrir.
### Actualización

#### Sistema de actualización AUTOMÁTICA

- La aplicación consulta a diario sus propias GitHub Releases (`update.txt`) en lugar de las comprobaciones de SumatraPDF upstream; descarga, valida (cabecera MZ + tamaño) y se reemplaza a sí misma (portable) o ejecuta el instalador (instalado).
- Totalmente aislada de SumatraPDF upstream: directorio, claves de registro, archivo de ajustes y nombre del exe propios.

---
## Descarga y sistema de actualización

- **Releases:** compiladas por GitHub Actions a partir de etiquetas `v*` — descarga `TumatraPDF-portable-win64.exe` (solo ejecutar) o `TumatraPDF-install-win64.exe` (instalador) desde [Releases](https://github.com/FelipeTesta/TumatraPDF/releases)
- **Actualización automática en la app:** comprobación diaria del `update.txt` del repositorio; cuando existe una versión más nueva, la app la descarga, la valida y se reemplaza a sí misma (portable) o ejecuta el instalador (instalado). Sin instalador y sin permisos de administrador para la versión portable.
- **Aislamiento:** instalar TumatraPDF NO toca una instalación existente de SumatraPDF upstream (directorios, claves de registro, nombres de exe y de ajustes separados).
- **Privacidad:** la única llamada de red que la app hace por su cuenta es el GET de comprobación de actualización a este repositorio. Se eliminó toda la recolección de datos de SumatraPDF upstream (subida de fallos, descarga de símbolos, enlaces de traducción) — los volcados de fallos quedan locales.

### Windows puede preguntar antes de ejecutar

TumatraPDF no está firmado con código, así que la primera vez que Windows ejecuta un archivo descargado de internet puede mostrar una advertencia ("Windows protegió tu PC") y pedir confirmación.

Haz clic en "Más información" -> "Ejecutar de todos modos"; o haz clic derecho en el archivo -> Propiedades -> marca "Desbloquear" al final -> Aplicar; o en PowerShell: `Unblock-File .\TumatraPDF-portable-win64.exe`.

El instalador descargado lleva la marca de la Web (advierte una vez al ejecutarlo); la app que instala se extrae localmente desde el payload embebido y nunca lleva la marca, así que la app instalada no advierte. El portable es el archivo descargado — conserva la marca y hay que desbloquearlo una vez ("Más información -> Ejecutar de todos modos" / Propiedades -> Desbloquear); eliminamos el auto-desbloqueo al inicio porque es una solución ineficaz para SmartScreen en un binario sin firmar Y un detonante clásico de falsos positivos en antivirus. Windows recuerda la aprobación para ese archivo.

**Algunos productos antivirus también marcan compilaciones sin firmar — ver la siguiente sección.**

### Falsos positivos de antivirus en compilaciones sin firmar

**Los binarios de las versiones no están firmados con código.** SumatraPDF upstream tampoco está firmado — su documentación indica que los falsos positivos de antivirus son comunes en binarios de Windows sin firmar. Por eso, Windows SmartScreen puede avisar, y los productos antivirus que dependen de heurísticas (motores de "comportamiento" / "control de amenazas avanzado") pueden marcar un binario recién publicado y sin firmar como sospechoso. Una detección en una compilación sin firmar es, en la mayoría de los casos, un falso positivo, no una infección.

**Qué cambió para que la compilación actual pase limpio:** la app ya no borra su propia marca de la Web (Mark of the Web) al iniciar, y la auto-actualización ya no lanza un `.bat` auxiliar a través de `cmd.exe` — ambos comportamientos son exactamente lo que los motores heurísticos penalizan. Las compilaciones actuales pasan limpias en la máquina que reportó el problema (0 cuarentenas, 0 reportes de fallo).

**Cómo verificar que el archivo es nuestro:** cada versión publica el SHA-256 de ambos archivos (el portable y el instalador) en las notas de la versión. Si el hash de tu archivo coincide, el archivo es byte a byte lo que publicamos y cualquier detección es un falso positivo.

**Qué hacer:** agrega una exclusión para el archivo (o su carpeta) en tu antivirus — **no** desactives la protección. Si sigue marcando, abre un issue con el nombre del producto antivirus y el ID de la detección para que podamos actuar.

**Caso concreto:** en noviembre de 2026 un antivirus (el "Advanced Threat Control" de Bitdefender) puso en cuarentena una versión publicada — detección `ATC.Malicious`, id `SuspiciousBehavior.54B279F15D24AA5B` — por los dos comportamientos descritos arriba; tras la corrección, los nuevos binarios ya no lo disparan. Ver `SECURITY.md` para el informe completo del incidente.

## Arquitectura

Basado en SumatraPDF:
- **Lenguaje:** C, C++
- **Compilación:** Premake5 → Visual Studio 2022
- **Interfaz:** API nativa Win32
- **Motor PDF:** MuPDF
- **Formatos:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu, Markdown (.md/.markdown)

## Estado

Historial + correcciones: ver [CHANGELOG.md](CHANGELOG.md) y [LOG.md](LOG.md).
