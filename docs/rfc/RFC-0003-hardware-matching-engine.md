# RFC-0003: Hardware Matching Engine

## Estado

Propuesto para Project Atlas v0.2.

No forma parte de la versión v0.1 ni de RC1.

## Problema

Project Atlas mantiene dos fuentes de información independientes:

1. El Device Registry dinámico de PipeWire.
2. La Hardware Database persistente almacenada en `hardware/`.

`audio-profile devices` muestra identificadores temporales de PipeWire, mientras
que `audio-profile hardware-info` requiere un identificador persistente de la
Hardware Database.

Ejemplo:

```text
PipeWire Device ID: 53
Hardware DB ID:     ca0132
```

## Principio arquitectónico

La Hardware Database es persistente.

El Device Registry es dinámico.

Un dispositivo detectado debe buscar primero una entrada compatible. Si ya
existe, debe asociarse con ella y nunca crear un duplicado.

## Flujo previsto

```text
PipeWire Registry
        |
        v
PpmDevice descubierto
        |
        v
Hardware Matching Engine
        |
        +-- Coincidencia encontrada
        |       |
        |       v
        |   Reutilizar Hardware DB ID existente
        |
        +-- Sin coincidencia
                |
                v
            Informar al usuario
```

## Responsabilidades

El Hardware Matching Engine deberá:

- recibir un `PpmDevice`;
- inspeccionar atributos estables;
- buscar coincidencias en `hardware/`;
- devolver el identificador persistente cuando exista;
- evitar entradas duplicadas;
- distinguir coincidencia, ausencia y ambigüedad;
- no usar el ID numérico de PipeWire como clave persistente.

## Identidad del hardware

Las coincidencias podrán considerar:

- API;
- nombre;
- descripción;
- ruta del objeto;
- fabricante;
- modelo;
- chip;
- identificadores ALSA;
- identificadores USB;
- otros atributos estables disponibles.

## API pública propuesta

La API exacta se definirá durante v0.2.

Ejemplo conceptual:

```c
int ppm_hardware_match(
    const PpmDevice *device,
    char *hardware_id,
    size_t hardware_id_size);
```

Resultados conceptuales:

```text
0   Coincidencia única encontrada
1   No existe coincidencia
2   Coincidencia ambigua
-1  Error
```

## Comportamiento de la CLI

Cuando exista una coincidencia:

```text
53  alsa  CA0132 Sound Core3D

Hardware DB:
    ca0132

Información:
    audio-profile hardware-info ca0132
```

Cuando no exista:

```text
66  v4l2  HD Pro Webcam C920

Hardware DB:
    no registrada
```

## Prevención de duplicados

Antes de crear una entrada futura, Project Atlas deberá:

1. buscar coincidencias exactas;
2. buscar alias conocidos;
3. detectar posibles coincidencias parciales;
4. solicitar confirmación ante ambigüedad;
5. crear una entrada solamente cuando no exista una compatible.

## Fuera de alcance para v0.1

No se implementará durante RC1:

- `hardware-register`;
- creación automática de JSON;
- modificación de `PpmDevice`;
- cambios en las APIs públicas;
- asociación automática entre Device Registry y Hardware DB.

## Plan para v0.2

1. Definir el modelo de entrada de Hardware DB.
2. Definir reglas de identidad estables.
3. Implementar lectura segura de la base de datos.
4. Implementar el motor de coincidencias.
5. Integrarlo con `audio-profile devices`.
6. Añadir pruebas unitarias de coincidencia y duplicados.
7. Diseñar posteriormente el flujo de creación de entradas.
