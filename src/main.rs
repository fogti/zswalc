#[macro_use]
extern crate log;

use {
    futures::{future::FutureExt, stream::StreamExt},
    hyper::{header, service, Body, Method, Request, Response, StatusCode},
    indoc::indoc,
    std::{collections::HashMap, net::SocketAddr, sync::Arc},
    tera::Tera,
    url::Url,
};

struct GlobalData {
    data_home: String,
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
            Arg::with_name("data-home")
                .long("data-home")
                .required(true)
                .help("sets the directory where to store the chat data"),
        )
        .arg(
            Arg::with_name("serv–addr")
                .long("serv-addr")
                .required(true)
                .help("sets the server address to bind/listen to"),
        )
        .arg(
            Arg::with_name("vroot")
                .long("vroot")
                .required(true)
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
        data_home: matches.value_of("data-home").unwrap().to_string(),
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
    let (mut parts, body) = req.into_parts();

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
            let msg = if entire_body.len() > 2 && entire_body.slice(..3) == &b"in="[..] {
                entire_body.slice(3..)
            } else {
                entire_body
            };
            Ok(post_msg(&data, real_path, &msg))
        }
        (Method::GET, _) => {
            let params: HashMap<_, _> = Url::parse(&parts.uri.to_string())
                .unwrap()
                .query_pairs()
                .collect();
            unimplemented!()
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

fn post_msg(gda: &GlobalData, chat: &str, data: &[u8]) -> Response<Body> {
    unimplemented!()
}

fn get_chat_data(gda: &GlobalData, chat: &str) -> Response<Body> {
    unimplemented!()
}

fn get_chat_data(gda: &GlobalData, chat: &str) -> Response<Body> {
    unimplemented!()
}
