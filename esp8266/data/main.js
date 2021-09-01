var urls = '%URLS%';
var pinstate = '%PINSTATE%';
var schedule = '%SCHEDULE%';
var lastStartup = '%LAST_STARTUP%';

function logout(){
    axios({
        url: "index.html",
        method: "post",
        withCredentials: true,
        auth: {
            username: 'log',
            password: 'out'
        }
    })
    .catch(
        window.location.replace("logout.html")
    )
}