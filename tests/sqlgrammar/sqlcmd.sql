INSERT INTO R21 VALUES (393, 0, 694, NULL, 1505034, NULL, -71.11874167, '7', '2a', 'yli%', 'b0', '{"JN4":[{"JN2":-9307.2e1}]}', NULL, x'66', NULL, PolygonFromText('POLYGON((-2 -2, 10 -2, -2 10, -2 -2),(-1 -1, -1 1, 3 -1, -1 -1))'), '144.227.88.67', NULL, x'0876C7B4', x'5D746b29655bB04919524944ccd7b5D0');
UPSERT INTO R4 (VCHAR_JSON, ID)        VALUES ((SELECT MIN(VCHAR_JSON)  FROM R3    ),      -177);
--
UPDATE R21 T0     SET POLYGON   = T0.POLYGON;
UPDATE R4 SET SMALL   = (SELECT COUNT(*)                                    FROM R21    WHERE R4.VARBIN = VARBIN )  WHERE NOT  TIME IN (SELECT   TO_TIMESTAMP(MICROS, ID) FROM R4     LIMIT 8 );
