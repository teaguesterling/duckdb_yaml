-- Advanced YAML functions test

-- Register YAML extension
LOAD yaml;

-- Test handling of complex YAML structures
CREATE TABLE complex_yaml(yaml VARCHAR);
INSERT INTO complex_yaml VALUES ('
# Nested arrays and objects
products:
  - id: 1
    name: Product 1
    price: 10.99
    tags: [electronics, gadget]
    details:
      weight: 1.5
      dimensions: [10, 20, 30]
      colors:
        - name: red
          hex: "#FF0000"
        - name: blue
          hex: "#0000FF"
  - id: 2
    name: Product 2
    price: 19.99
    tags: [clothing, apparel]
    details:
      weight: 0.5
      dimensions: [5, 10, 2]
      colors:
        - name: black
          hex: "#000000"
        - name: white
          hex: "#FFFFFF"
');

-- Cast to JSON
SELECT yaml::JSON AS json_value FROM complex_yaml;

-- Query using JSON functions after conversion
WITH yaml_as_json AS (
    SELECT yaml::JSON AS json FROM complex_yaml
)
SELECT 
    json_extract_int(json, '$.products[0].id') AS first_product_id,
    json_extract_string(json, '$.products[0].name') AS first_product_name,
    json_extract_string(json, '$.products[0].tags[0]') AS first_product_first_tag,
    json_extract_string(json, '$.products[0].details.colors[0].name') AS first_product_first_color
FROM yaml_as_json;

-- Test reading complex YAML as a table
CREATE TABLE products AS 
    SELECT * FROM read_yaml('
products:
  - id: 1
    name: Product 1
    price: 10.99
    in_stock: true
  - id: 2
    name: Product 2
    price: 19.99
    in_stock: false
  - id: 3
    name: Product 3
    price: 5.99
    in_stock: true
');

-- Show the structure
DESCRIBE products;

-- Query the data
SELECT products FROM products;

-- Explode the products array
SELECT 
    p.id, 
    p.name, 
    p.price, 
    p.in_stock
FROM products, UNNEST(products.products) AS p;

-- Test with explicit column types
CREATE TABLE products_typed AS 
    SELECT * FROM read_yaml('
products:
  - id: 1
    name: Product 1
    price: 10.99
    in_stock: true
  - id: 2
    name: Product 2
    price: 19.99
    in_stock: false
  - id: 3
    name: Product 3
    price: 5.99
    in_stock: true
', columns={products: 'LIST(STRUCT(id INTEGER, name VARCHAR, price DOUBLE, in_stock BOOLEAN))'});

-- Verify the types
DESCRIBE products_typed;

-- Test error handling with ignore_errors
CREATE TABLE with_errors AS 
    SELECT * FROM read_yaml('
valid:
  - id: 1
    value: 100
  - id: 2
    value: invalid_number
  - id: 3
    value: 300
', ignore_errors=true, columns={valid: 'LIST(STRUCT(id INTEGER, value INTEGER))'});

SELECT valid FROM with_errors;

-- Test multi-document YAML with different structures
CREATE TABLE different_docs AS 
    SELECT * FROM read_yaml('
---
type: user
id: 1
name: John
---
type: product
id: 101
name: Laptop
price: 999.99
---
type: order
id: 1001
items:
  - product_id: 101
    quantity: 2
', multi_document=true);

SELECT * FROM different_docs;

-- Clean up
DROP TABLE complex_yaml;
DROP TABLE products;
DROP TABLE products_typed;
DROP TABLE with_errors;
DROP TABLE different_docs;
