# PlumbrC Pattern Library

This directory contains **702 patterns** for secret detection, imported from the Plumbr Zig project.

## Directory Structure

```
patterns/
├── all.txt              # Combined all 702 patterns
├── default.txt          # Quick-start 42 patterns
├── README.md
│
├── cloud/               # 109 patterns
│   ├── aws.txt          
│   ├── gcp.txt          
│   ├── azure.txt        
│   └── cloud_all.txt    # AWS, GCP, Azure, DigitalOcean, Heroku, Alibaba, Oracle
│
├── vcs/                 # 63 patterns
│   ├── github.txt       
│   ├── gitlab.txt       
│   ├── bitbucket.txt    
│   └── vcs_all.txt      # All VCS tokens
│
├── payment/             # 79 patterns
│   ├── stripe.txt       
│   ├── paypal.txt       
│   ├── cards.txt        
│   └── payment_all.txt  # Stripe, Square, PayPal, Braintree, Adyen, Checkout
│
├── communication/       # 127 patterns
│   ├── slack.txt        
│   ├── discord.txt      
│   ├── messaging.txt    
│   ├── comm_all.txt     # Slack, Discord, Telegram, Twilio
│   └── email.txt        # SendGrid, Mailgun, Mailchimp, SES, Postmark
│
├── database/            # 42 patterns
│   ├── connections.txt  
│   └── db_all.txt       # PostgreSQL, MongoDB, MySQL, Redis
│
├── crypto/              # 54 patterns
│   ├── keys.txt         
│   └── crypto_all.txt   # SSH, SSL/TLS, PGP, JWT
│
├── auth/                # 10 patterns
│   └── tokens.txt       # JWT, OAuth, Bearer tokens
│
├── secrets/             # 52 patterns
│   ├── generic.txt      
│   └── secrets_all.txt  # API keys, passwords, tokens
│
├── pii/                 # 14 patterns
│   └── personal.txt     # Email, SSN, phone, IP
│
├── infra/               # 72 patterns
│   ├── devops.txt       
│   └── cicd.txt         # NPM, PyPI, Docker, GitHub Actions, CircleCI, Jenkins
│
├── social/              # 50 patterns
│   └── social.txt       # Facebook, Twitter, LinkedIn, YouTube, TikTok, Reddit
│
└── analytics/           # 30 patterns
    └── analytics.txt    # Google Analytics, Mixpanel, Amplitude, Segment, Heap
```

## Pattern Count by Category

| Category | Patterns | Coverage |
|----------|----------|----------|
| Cloud | 109 | AWS, GCP, Azure, DigitalOcean, Heroku, Alibaba, Oracle |
| Communication | 127 | Slack, Discord, Telegram, Twilio, SendGrid, Mailgun, etc. |
| Payment | 79 | Stripe, Square, PayPal, Braintree, Adyen, Checkout.com |
| VCS | 63 | GitHub, GitLab, Bitbucket |
| Infrastructure | 72 | Docker, NPM, PyPI, CI/CD pipelines |
| Crypto | 54 | SSH, SSL/TLS, PGP, JWT |
| Secrets | 52 | Generic API keys, passwords, tokens |
| Social | 50 | Facebook, Twitter, LinkedIn, YouTube, TikTok, Reddit |
| Database | 42 | PostgreSQL, MongoDB, MySQL, Redis |
| Analytics | 30 | Google Analytics, Mixpanel, Amplitude, Segment |
| PII | 14 | Personal information |
| Auth | 10 | Authentication tokens |
| **Total** | **702** | |

## Usage

### Load all 702 patterns
```bash
./plumbr -p patterns/all.txt < input.log
```

### Load specific category
```bash
./plumbr -p patterns/cloud/aws.txt < input.log
```

### Combine multiple files
```bash
./plumbr -p patterns/cloud/aws.txt -p patterns/pii/personal.txt < input.log
```

### Use defaults plus extras
```bash
./plumbr -d -p patterns/social/social.txt < input.log
```

## Pattern Format

```
name|literal|regex|replacement
```

- **name**: Unique identifier for the pattern
- **literal**: Fast literal string for Aho-Corasick pre-filtering
- **regex**: PCRE2 regular expression for precise matching
- **replacement**: Replacement text (e.g., `[REDACTED:category]`)

## Origins

Patterns are converted from the [Plumbr](https://github.com/yourorg/plumbr) Zig project's pattern library.
