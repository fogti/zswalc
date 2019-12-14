#[macro_use]
extern crate log;

use {
    futures::future::FutureExt,
    hyper::{header, service, Body, Method, Request, Response, StatusCode},
    indoc::indoc,
    r2d2_sqlite::SqliteConnectionManager,
    rusqlite::{params, OptionalExtension},
    std::{collections::HashMap, net::SocketAddr, sync::Arc},
    tera::Tera,
    url::Url,
};

struct GlobalData {
    db: r2d2::Pool<SqliteConnectionManager>,
    vroot: String,
    tera: Tera,
}

#[tokio::main]
async fn main() {
    // 0. check args
    use clap::Arg;
    let matches = clap::App::new("zswalc")
        .version(clap::crate_version!())
        .author("Erik Zscheile <erik.zscheile@gmail.com>")
        .about("Zscheile Web Application Light Chat is an experimental chatting app")
        .arg(
            Arg::with_name("database")
                .short("b")
                .long("database")
                .required(true)
                .takes_value(true)
                .help("sets the file where to store the chat data"),
        )
        .arg(
            Arg::with_name("serv–addr")
                .short("a")
                .long("serv-addr")
                .required(true)
                .takes_value(true)
                .help("sets the server address to bind/listen to"),
        )
        .arg(
            Arg::with_name("vroot")
                .short("r")
                .long("vroot")
                .takes_value(true)
                .help("sets the HTTP base path of this app (defaults to '')"),
        )
        .get_matches();

    // 1. prevent periodic stat(/etc/localtime)
    {
        let timezone = std::fs::read("/etc/timezone").expect("unable to read timezone file");
        let encoded =
            os_str_bytes::OsStrBytes::from_bytes(&timezone[..]).expect("got invalid timezone data");
        std::env::set_var("TZ", encoded);
    }

    // 2. initialize rest
    pretty_env_logger::init();
    let serv_addr: SocketAddr = matches
        .value_of("serv-addr")
        .unwrap()
        .parse()
        .expect("got invalid server address");

    let gld = Arc::new(GlobalData {
        db: r2d2::Pool::new(SqliteConnectionManager::file(
            matches.value_of("data-home").unwrap().to_string(),
        ))
        .expect("unable to open database"),
        vroot: matches.value_of("vroot").unwrap_or("").to_string(),
        tera: {
            let mut tera = Tera::default();
            tera.add_raw_templates(vec![(
                "pagetemplate.html",
                include_str!("../zswebapp/pagetemplate.html"),
            )])
            .expect("failed to load templates");
            tera
        },
    });

    gld.db
        .get()
        .unwrap()
        .execute_batch(indoc!(
            "
            BEGIN;
            CREATE TABLE IF NOT EXISTS chats (
              id INTEGER PRIMARY KEY,
              name TEXT NOT NULL,
            );
            CREATE TABLE IF NOT EXISTS msgs (
              id INTEGER PRIMARY KEY,
              chat INTEGER NOT NULL,
              user TEXT NOT NULL,
              timestamp TEXT NOT NULL,
              content TEXT NOT NULL,
            );
            COMMIT;
            "
        ))
        .expect("failed to initialize database");

    // 3. run server
    let serv_fn = service::make_service_fn(move |_conn| {
        let gld = gld.clone();
        let inner_fn = move |req| router(req, gld.clone()).boxed();
        async move { Ok::<_, std::convert::Infallible>(service::service_fn(inner_fn)) }
    });
    let serve_future = hyper::Server::bind(&serv_addr).serve(serv_fn);

    if let Err(e) = serve_future.await {
        error!("{}", e);
    }
}

async fn router(req: Request<Body>, data: Arc<GlobalData>) -> Result<Response<Body>, hyper::Error> {
    let (parts, body) = req.into_parts();

    // check path
    let mut real_path = parts.uri.path();
    if !real_path.starts_with(&data.vroot) {
        return Ok(if real_path == "/" {
            // forward to real root
            Response::builder()
                .header("Location", data.vroot.clone() + "/")
                .status(StatusCode::MOVED_PERMANENTLY)
                .body(Body::from(""))
                .unwrap()
        } else {
            // invalid path
            format_error(StatusCode::NOT_FOUND, "file not found")
        });
    }
    // strip prefix and postfix
    real_path = real_path
        .get(data.vroot.len()..)
        .unwrap()
        .trim_end_matches('/');

    // TODO: authentification
    let user = "&lt;anon&gt;";

    match (parts.method.clone(), real_path) {
        (Method::GET, "/static/zlc.js") => {
            let body = Body::from(include_str!("../zswebapp/zlc.js"));
            Ok(Response::builder()
                .status(StatusCode::OK)
                .header(header::CONTENT_TYPE, "text/javascript")
                .body(body)
                .unwrap())
        }
        (_, p) if p.starts_with("/static/") || p == "/static" => Ok(Response::builder()
            .status(StatusCode::METHOD_NOT_ALLOWED)
            .body(Body::from(""))
            .unwrap()),

        (Method::POST, _) => {
            let entire_body = hyper::body::to_bytes(body).await?;
            Ok(handle_res(post_msg(&data, real_path, user, &entire_body)))
        }
        (Method::GET, _) => {
            let url_ = Url::parse(&parts.uri.to_string()).unwrap();
            let params: HashMap<_, _> = url_.query_pairs().collect();
            let lower_bound = params
                .get("lower_bound")
                .and_then(|i| i.parse::<i64>().ok());
            let upper_bound = params
                .get("upper_bound")
                .and_then(|i| i.parse::<i64>().ok());
            Ok(
                if lower_bound.is_some()
                    || upper_bound.is_some()
                    || params
                        .get("lower_bound")
                        .map(|i| i == "cur")
                        .unwrap_or(false)
                {
                    handle_res(get_chat_data(&data, real_path, lower_bound, upper_bound))
                } else {
                    get_chat_page(
                        &data,
                        real_path,
                        user,
                        params.get("show_chat").map(|i| i.as_ref()),
                    )
                },
            )
        }

        (_, _) => Ok(format_error(StatusCode::NOT_FOUND, "file not found")),
    }
}

fn format_error(stc: StatusCode, msg: &str) -> Response<Body> {
    let body = Body::from(format!(
        "{} {}{}\n",
        indoc!(
            r#"
            <!doctype html>
            <html>
            <head><title>ERROR occured in chat app</title></head>
            <body>
              <h1>ERROR in chat app</h1>
              <p style="color: red;">Error:
            "#
        ),
        msg,
        indoc!(
            r#"
              </p>
            </body>
            </html>
            "#
        ),
    ));

    Response::builder()
        .status(stc)
        .header(header::CONTENT_TYPE, "text/javascript")
        .body(body)
        .unwrap()
}

fn chatname_to_id(gda: &GlobalData, chat: &str) -> rusqlite::Result<i64> {
    let db = gda.db.get().unwrap();
    let mut stmt = db.prepare_cached("SELECT id FROM chats WHERE name = ?1")?;
    stmt.query_row(params![chat], |row| row.get(0))
        .optional()?
        .ok_or_else(|| {
            // create new chat-room
            db.execute("INSERT INTO chats (name) VALUES (?1)", params![chat])?;
            stmt.query_row(params![chat], |row| row.get(0))
        })
        .or_else(std::convert::identity)
}

fn handle_err<E: std::fmt::Debug>(x: E) -> Response<Body> {
    format_error(StatusCode::INTERNAL_SERVER_ERROR, &format!("{:?}", x))
}

fn handle_res<E: std::fmt::Debug>(x: Result<Response<Body>, E>) -> Response<Body> {
    x.unwrap_or_else(handle_err)
}

fn post_msg(
    gda: &GlobalData,
    chat: &str,
    user: &str,
    data: &[u8],
) -> rusqlite::Result<Response<Body>> {
    let chatid = chatname_to_id(gda, chat)?;
    let tstamp = format!("{}", chrono::Utc::now().format("%F %T%.3f"));
    let data = match std::str::from_utf8(data) {
        Ok(x) => x,
        Err(x) => return Ok(handle_err(x)),
    };

    gda.db.get().unwrap().execute(
        "INSERT INTO msgs (chat, user, timestamp, content) VALUES (?1, ?2, ?3)",
        params![chatid, user, tstamp, data],
    )?;

    Ok(Response::builder()
        .status(StatusCode::OK)
        .body(Body::from(""))
        .unwrap())
}

fn get_chat_data(
    gda: &GlobalData,
    chat: &str,
    lower_bound: Option<i64>,
    upper_bound: Option<i64>,
) -> rusqlite::Result<Response<Body>> {
    use {rusqlite::ToSql, std::cmp};

    let chatid = chatname_to_id(gda, chat)?;

    // prepare statement
    let mut stmt = "SELECT (id, user, timestamp, content) FROM msgs WHERE chat = :chat".to_string();
    if lower_bound.is_some() {
        stmt += " AND id > :lob";
    }
    if upper_bound.is_some() {
        stmt += " AND id < :upb";
    }
    stmt += " ORDER BY id DESC";

    let db = gda.db.get().unwrap();
    let mut stmt = db.prepare_cached(&stmt)?;

    // execute statement
    let mut pars = Vec::with_capacity(3);
    pars.push(("chat", &chatid as &dyn ToSql));
    if let Some(lob) = lower_bound.as_ref() {
        pars.push(("lob", lob as &dyn ToSql));
    }
    if let Some(upb) = upper_bound.as_ref() {
        pars.push(("upb", upb as &dyn ToSql));
    }
    let mut new_bounds: Option<(i64, i64)> = None;
    let xiter = stmt.query_map_named(&pars[..], |row| {
        let id: i64 = row.get(0)?;
        let user: String = row.get(1)?;
        let tstamp: String = row.get(2)?;
        let content: String = row.get(3)?;
        // calculate new bounds
        new_bounds = Some(match new_bounds {
            None => (id, id),
            Some((lob, upb)) => (cmp::min(lob, id), cmp::max(upb, id)),
        });
        // generate message HTML
        Ok(format!(
            "<a name=\"e{}\">[{}]</a>{} ({}): {}<br />\n",
            id, id, user, tstamp, content
        ))
    })?;

    let mut bdat = Vec::new();
    if lower_bound.is_none() && upper_bound.is_none() {
        // only fetch the last 20 messages per default
        for i in xiter.take(20) {
            bdat.push(i?);
        }
    } else {
        for i in xiter {
            bdat.push(i?);
        }
    }
    let mut rb = Response::builder();
    if let Some(bs) = new_bounds {
        use {hyper::header::HeaderValue, std::convert::TryFrom};
        let headers = rb.headers_mut().unwrap();
        headers.insert(
            "X-FirstMsgId",
            HeaderValue::try_from(format!("{}", bs.0)).unwrap(),
        );
        headers.insert(
            "X-LastMsgId",
            HeaderValue::try_from(format!("{}", bs.1)).unwrap(),
        );
    }
    Ok(rb
        .status(StatusCode::OK)
        .body(Body::from(bdat.join("")))
        .unwrap())
}

fn get_chat_page(
    gda: &GlobalData,
    _chat: &str,
    user: &str,
    show_chat: Option<&str>,
) -> Response<Body> {
    let mut ctx = tera::Context::new();
    ctx.insert("user", user);
    if let Some(x) = show_chat {
        ctx.insert("show_chat", x);
    }
    Response::builder()
        .status(StatusCode::OK)
        .body(Body::from(
            gda.tera
                .render("pagetemplate.html", &ctx)
                .expect("tera render failed")
                .to_string(),
        ))
        .unwrap()
}
