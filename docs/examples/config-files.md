# Configuration File Examples

Examples of working with configuration files in YAML format.

## Application Configuration

### Reading a Config File

Given `app.yaml`:

```yaml
app:
  name: MyApp
  version: 1.2.3
  environment: production

database:
  host: db.example.com
  port: 5432
  name: myapp_prod
  pool_size: 10

features:
  authentication: true
  logging: true
  metrics: true
  rate_limiting: false

cache:
  provider: redis
  host: cache.example.com
  ttl: 3600
```

Query the configuration:

```sql
-- Read and extract values
SELECT
    yaml_extract_string(data, '$.app.name') AS app_name,
    yaml_extract_string(data, '$.app.version') AS version,
    yaml_extract_string(data, '$.database.host') AS db_host,
    yaml_extract(data, '$.database.port')::INTEGER AS db_port
FROM read_yaml_objects('app.yaml');

-- Check feature flags
SELECT
    yaml_extract(data, '$.features.authentication')::BOOLEAN AS auth_enabled,
    yaml_extract(data, '$.features.rate_limiting')::BOOLEAN AS rate_limit
FROM read_yaml_objects('app.yaml');
```

### Multi-Environment Configs

Given multiple environment files:

```yaml
# production.yaml
environment: production
database:
  host: db.prod.example.com
log_level: warn

# staging.yaml
environment: staging
database:
  host: db.staging.example.com
log_level: info

# development.yaml
environment: development
database:
  host: localhost
log_level: debug
```

Compare environments:

```sql
SELECT
    yaml_extract_string(data, '$.environment') AS env,
    yaml_extract_string(data, '$.database.host') AS db_host,
    yaml_extract_string(data, '$.log_level') AS log_level
FROM read_yaml_objects('*.yaml', filename = true)
ORDER BY env;
```

---

## Infrastructure as Code

### Kubernetes Configuration

Parse Kubernetes manifests:

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-app
  labels:
    app: my-app
spec:
  replicas: 3
  selector:
    matchLabels:
      app: my-app
  template:
    spec:
      containers:
        - name: app
          image: my-app:v1.0.0
          ports:
            - containerPort: 8080
          resources:
            limits:
              cpu: "500m"
              memory: "128Mi"
```

```sql
-- Analyze deployments
SELECT
    yaml_extract_string(data, '$.metadata.name') AS name,
    yaml_extract_string(data, '$.kind') AS kind,
    yaml_extract(data, '$.spec.replicas')::INTEGER AS replicas,
    yaml_extract_string(data, '$.spec.template.spec.containers[0].image') AS image
FROM read_yaml_objects('k8s/*.yaml')
WHERE yaml_extract_string(data, '$.kind') = 'Deployment';
```

### Docker Compose

Parse `docker-compose.yaml`:

```yaml
version: '3.8'
services:
  web:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./html:/usr/share/nginx/html
  api:
    build: ./api
    ports:
      - "3000:3000"
    environment:
      DATABASE_URL: postgres://db:5432/app
  db:
    image: postgres:14
    volumes:
      - pgdata:/var/lib/postgresql/data
volumes:
  pgdata:
```

```sql
-- List services
SELECT key AS service_name,
       yaml_extract_string(value, '$.image') AS image,
       yaml_extract(value, '$.ports') AS ports
FROM yaml_each(
    yaml_extract(
        (SELECT data FROM read_yaml_objects('docker-compose.yaml')),
        '$.services'
    )
);
```

---

## CI/CD Configuration

### GitHub Actions

Parse workflow files:

```yaml
# .github/workflows/ci.yaml
name: CI Pipeline
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: make build
  test:
    runs-on: ubuntu-latest
    needs: build
    steps:
      - uses: actions/checkout@v3
      - name: Test
        run: make test
```

```sql
-- Analyze workflow jobs
SELECT
    yaml_extract_string(data, '$.name') AS workflow_name,
    yaml_keys(yaml_extract(data, '$.jobs')) AS job_names
FROM read_yaml_objects('.github/workflows/*.yaml');

-- Get job details
SELECT
    key AS job_name,
    yaml_extract_string(value, '$.runs-on') AS runner,
    yaml_extract(value, '$.needs') AS dependencies
FROM read_yaml_objects('.github/workflows/ci.yaml'),
     yaml_each(yaml_extract(data, '$.jobs'));
```

---

## Configuration Validation

### Schema Validation

```sql
-- Check required fields
SELECT
    filename,
    yaml_exists(data, '$.app.name') AS has_app_name,
    yaml_exists(data, '$.database.host') AS has_db_host,
    yaml_exists(data, '$.database.port') AS has_db_port
FROM read_yaml_objects('configs/*.yaml', filename = true);

-- Find missing required fields
SELECT filename
FROM read_yaml_objects('configs/*.yaml', filename = true)
WHERE NOT yaml_exists(data, '$.database.host')
   OR NOT yaml_exists(data, '$.database.port');
```

### Type Validation

```sql
-- Validate types
SELECT
    filename,
    yaml_type(yaml_extract(data, '$.database.port')) AS port_type,
    yaml_type(yaml_extract(data, '$.features')) AS features_type
FROM read_yaml_objects('configs/*.yaml', filename = true)
WHERE yaml_type(yaml_extract(data, '$.database.port')) != 'scalar'
   OR yaml_type(yaml_extract(data, '$.features')) != 'object';
```

---

## Configuration Export

### Generate Config from Database

```sql
-- Export application settings
COPY (
    SELECT value_to_yaml(struct_pack(
        app := struct_pack(
            name := app_name,
            version := version,
            environment := environment
        ),
        database := struct_pack(
            host := db_host,
            port := db_port,
            name := db_name
        ),
        features := list(feature_name)
    ))
    FROM app_config
    JOIN features USING (app_id)
    GROUP BY app_id
) TO 'generated_config.yaml' (FORMAT yaml, STYLE block);
```

### Merge Configurations

```sql
-- Combine base config with overrides
SELECT yaml_to_json(base.data) || yaml_to_json(override.data) AS merged
FROM read_yaml_objects('base.yaml') base,
     read_yaml_objects('override.yaml') override;
```
