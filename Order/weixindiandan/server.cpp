//
// Created by rkasasa on 23-1-27.
//

#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#include "httplib.h"
#include "order_system.hpp"
#include "jsoncpp/json/json.h"
#include "bits/stdc++.h"
using namespace httplib;
using namespace order_system;
using namespace std;
pair<int,int> dish_number_substr(string s)
{
    int pos = s.find(':');
    int dish_id=atoi(s.substr(0,pos).c_str()),dish_number=atoi(s.substr(pos+1,s.size()).c_str());
    return {dish_id,dish_number};
}
bool GetDishes(const Json::Value& dish_ids, Json::Value* dishes,
               order_system::DishTable& dish_table) {
    for (uint32_t i = 0; i < dish_ids.size(); ++i) {
        LOG(INFO) << "处理第 " << i << " 个菜品" << std::endl;
        pair<int,int> pa= dish_number_substr(dish_ids[i].asString());
        int dish_id = pa.first;
        Json::Value dish_info;
        bool ret = dish_table.SelectOne(dish_id, &dish_info);
        dish_info["number"]=pa.second;
        if (!ret) {
            LOG(ERROR) << "dish_id = " << dish_id << " not found!\n";
            continue;
        }
        dishes->append(dish_info);
    }
    return true;
}
int GetConsume(const Json::Value& dishes) {
    int consume = 0;
    for (uint32_t i = 0; i < dishes.size(); ++i) {
        consume += dishes[i]["price"].asInt()*dishes[i]["number"].asInt();
    }
    return consume;
}

bool Dish_Number(order_system::DishTable& dish_table,const Json::Value& order)
{

    Json::Value dishes;
    bool ret = dish_table.SelectAll(&dishes);
    if(!ret) return false;
    map<int,int> dish_number;
    for (int i = 0; i < dishes.size(); ++i) {
        dish_number[dishes[i]["dish_id"].asInt()]=dishes[i]["number"].asInt();
    }
    bool ans=true;
    for (int i = 0; i < order["dish_ids"].size(); ++i) {
        pair<int,int> pa= dish_number_substr(order["dish_ids"][i].asString());
        if(!dish_number.count(pa.first)||dish_number[pa.first]<pa.second) ans = false;
    }
    return ans;
}
MYSQL* mysql = NULL;
int main()
{
    Server server;
    mysql = MySQLInit();
    signal(SIGINT, [](int) { MySQLRelease(mysql); exit(0);});
    DishTable dish_table(mysql);
    OrderTable order_table(mysql);
    server.Post("/dish", [&dish_table](const Request& req, Response& resp) {
        LOG(INFO)<< "新增菜品: " << req.body << std::endl;
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value req_json;
        Json::Value resp_json;
        bool ret = reader.parse(req.body,req_json);
        if(!ret)
        {
            resp_json["ok"] = false;
            resp_json["reason"] = "parse Request failed!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json),"applocation/json");
            return;
        }
        if(req_json["name"].empty()||req_json["price"].empty())
        {
            resp_json["ok"] = false;
            resp_json["reason"]="Requset has no name or price field!\n";
            resp.status=400;
            resp.set_content(writer.write(resp_json),"application/json");
            return;
        }

        ret = dish_table.Insert(req_json);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "Insert failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        resp_json["ok"] = true;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
    });
    server.Get("/dish", [&dish_table](const Request& req, Response& resp) {
        LOG(INFO) << "查看所有菜品: " << req.body << std::endl;
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value resp_json;
        Json::Value dishes;
        bool ret=false;
        ret = dish_table.SelectAll(&dishes);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "SelectAll failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        resp.set_content(writer.write(dishes), "application/json");
        return;
    });
    server.Delete(R"(/dish/(\d+))", [&dish_table](const Request& req, Response& resp) {
        Json::Value resp_json;
        Json::FastWriter writer;
        int dish_id = std::stoi(req.matches[1]);
        LOG(INFO) << "删除指定菜品: " << dish_id << std::endl;
        bool ret = dish_table.Delete(dish_id);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "SelectAll failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
        }
        resp_json["ok"] = true;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
    });
    server.Put(R"(/dish/(\d+))", [&dish_table](const Request& req, Response& resp) {
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value req_json;
        Json::Value resp_json;

        int dish_id = std::stoi(req.matches[1]);
        LOG(INFO) << "修改菜品 " << dish_id << "|" << req.body << std::endl;
        bool ret = reader.parse(req.body, req_json);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "parse Request failed!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        req_json["dish_id"] = dish_id;
        if (req_json["name"].empty() || req_json["price"].empty()) {
            resp_json["ok"] = false;
            resp_json["reason"] = "Request has no name or price!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }

        {
            muduo::MutexLockGuard lock(dish_table.mutex_);
            ret = dish_table.Update(req_json);
        }

        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "Update failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        resp_json["ok"] = true;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
    });
    server.Post("/order", [&dish_table,&order_table](const Request& req, Response& resp) {
        LOG(INFO) << "新增订单: " << req.body << std::endl;
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value req_json;
        Json::Value resp_json;
        bool ret = reader.parse(req.body, req_json);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "parse Request failed!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        if (req_json["table_id"].empty() || req_json["time"].empty()
            || req_json["dish_ids"].empty()) {
            resp_json["ok"] = false;
            resp_json["reason"] = "Request has no table_id or time or dish_ids!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        const Json::Value& dish_ids = req_json["dish_ids"];
        req_json["dish_ids_str"] = writer.write(dish_ids);
        {
            muduo::MutexLockGuard lock(dish_table.mutex_);
            ret = Dish_Number(dish_table,req_json);
            if(ret)
                ret = order_table.Insert(req_json);
            if(ret)
            {
                for (int i = 0; i < dish_ids.size(); ++i) {
                    pair<int,int> pa= dish_number_substr(dish_ids[i].asString());
                    ret=dish_table.Update_number(pa.first,pa.second);
                }
            }
        }
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "OrderTable Insert failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        resp_json["ok"] = true;
        resp.set_content(writer.write(resp_json), "application/json");
    });
    server.Put(R"(/order/(\d+))", [&order_table](const Request& req, Response& resp) {
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value req_json;
        Json::Value resp_json;
        int order_id = std::stoi(req.matches[1]);
        LOG(INFO) << "订单id " << order_id << "|" << req.body << std::endl;
        bool ret = reader.parse(req.body, req_json);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "parse Request failed!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        req_json["order_id"] = order_id;
        if (req_json["order_id"].empty() || req_json["state"].empty()) {
            resp_json["ok"] = false;
            resp_json["reason"] = "Request has no order_id or state!\n";
            resp.status = 400;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        ret = order_table.ChangeState(req_json);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "ChangeState failed\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        resp_json["ok"] = true;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
    });
    server.Get("/order", [&order_table, &dish_table](const Request& req, Response& resp) {
        LOG(INFO) << "查看所有订单: " << req.body << std::endl;
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value resp_json;
        Json::Value orders;
        bool ret = order_table.SelectAll(&orders);
        if (!ret) {
            resp_json["ok"] = false;
            resp_json["reason"] = "SelectAll failed!\n";
            resp.status = 500;
            resp.set_content(writer.write(resp_json), "application/json");
            return;
        }
        for (uint32_t order_index = 0; order_index < orders.size(); ++order_index) {
            LOG(INFO) << "处理第 " << order_index << " 个订单 ing" << std::endl;
            Json::Value& order = orders[order_index];
            Json::Value dish_ids;
            ret = reader.parse(order["dish_ids"].asString(), dish_ids);
            if (!ret) {
                LOG(ERROR) << "order_id: " << order["order_id"].asInt()
                           << " has error dish_ids!" << std::endl;
                continue;
            }
            GetDishes(dish_ids, &order["dishes"], dish_table);
            order["consume"] = GetConsume(order["dishes"]);
        }
        resp.set_content(writer.write(orders), "application/json");
        return;
    });
    server.set_base_dir("./wwwroot");
    server.listen("0.0.0.0", 9092);
    return 0;
}